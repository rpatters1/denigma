// Copyright 2026 Robert G. Patterson.
// SPDX-License-Identifier: MIT
//
// Exercises the WebAssembly module's C ABI end to end: inspection, every
// exporter, linked-part selection, EnigmaXML and zipped EnigmaXML input, and
// diagnostic reporting for invalid input. It has no dependencies beyond Node.
//
// usage: node tests/wasm/smoke.mjs <denigma.js> <denigma.wasm> [sample.musx]

import { readFile } from 'node:fs/promises';
import { resolve } from 'node:path';
import { pathToFileURL } from 'node:url';

const defaultSample = resolve(import.meta.dirname, '..', 'data', 'inputs', 'voiced_parts.musx');
const chordSample = resolve(import.meta.dirname, '..', 'data', 'inputs', 'chords.musx');
const [, , moduleArg, wasmArg, musxArg = defaultSample] = process.argv;
if (!moduleArg || !wasmArg) {
  console.error('usage: node tests/wasm/smoke.mjs <denigma.js> <denigma.wasm> [sample.musx]');
  process.exit(2);
}

// Output format and diagnostic severity codes from the C ABI in src/wasm/denigma_wasm.cpp.
const FORMAT_MUSICXML = 0;
const FORMAT_MNX = 1;
const FORMAT_ENIGMAXML = 2;
const SEVERITY_VERBOSE = 3;
const MNX_INDENT_SPACES = 2;

const createModule = (await import(pathToFileURL(resolve(moduleArg)))).default;
const Module = await createModule({ wasmBinary: await readFile(resolve(wasmArg)) });

console.log(`denigma ${Module.UTF8ToString(Module._denigma_version())} (${Module.UTF8ToString(Module._denigma_commit())})`);

function messages(result) {
  const count = Module._denigma_result_diagnostic_count(result);
  return Array.from({ length: count }, (_, index) =>
    Module.UTF8ToString(Module._denigma_result_diagnostic_message(result, index))).join('\n');
}

function withAllocated(bytes, callback) {
  const pointer = Module._denigma_malloc(bytes.byteLength);
  Module.HEAPU8.set(bytes, pointer);
  try {
    return callback(pointer);
  } finally {
    Module._denigma_free(pointer);
  }
}

function withInput(bytes, name, callback) {
  return withAllocated(bytes, (dataPointer) =>
    withAllocated(new TextEncoder().encode(`${name}\0`), (namePointer) => callback(dataPointer, namePointer)));
}

function withSelection(indices, callback) {
  const bytes = new Uint8Array(new Int32Array(indices).buffer);
  return withAllocated(bytes, (pointer) => callback(pointer, indices.length));
}

function convert(dataPointer, size, namePointer, format, options = {}) {
  const { includeTempo = 0, allFonts = 0, finaleRestPosition = 0, splitInstruments = 0, cueLayer = 0, selection = [] } = options;
  return withSelection(selection, (selectionPointer, selectionCount) =>
    Module._denigma_convert(dataPointer, size, namePointer, format, includeTempo, allFonts, finaleRestPosition,
      splitInstruments, MNX_INDENT_SPACES, cueLayer, selectionCount ? selectionPointer : 0, selectionCount));
}

function assertResult(result, label, marker, { outputCount = 1, verbose = false, indices } = {}) {
  let firstOutput;
  try {
    if (!Module._denigma_result_success(result)) throw new Error(`${label} failed:\n${messages(result)}`);
    if (verbose) {
      const count = Module._denigma_result_diagnostic_count(result);
      const severities = Array.from({ length: count }, (_, index) => Module._denigma_result_diagnostic_severity(result, index));
      if (!severities.includes(SEVERITY_VERBOSE)) throw new Error(`${label} did not return verbose diagnostics`);
    }
    const actualCount = Module._denigma_result_output_count(result);
    if (actualCount !== outputCount) throw new Error(`${label} produced ${actualCount} outputs; expected ${outputCount}`);
    if (indices) {
      const actual = Array.from({ length: actualCount }, (_, index) => Module._denigma_result_output_index(result, index));
      if (actual.some((value, index) => value !== indices[index])) {
        throw new Error(`${label} output indices were ${actual}; expected ${indices}`);
      }
    }
    const pointer = Module._denigma_result_output_data(result, 0);
    const size = Module._denigma_result_output_size(result, 0);
    firstOutput = Module.HEAPU8.slice(pointer, pointer + size);
    if (!new TextDecoder().decode(firstOutput).includes(marker)) throw new Error(`${label} output did not include ${marker}`);
    console.log(`${label}: ${size} bytes`);
  } finally {
    Module._denigma_result_destroy(result);
  }
  return firstOutput;
}

function gapReport(result) {
  const pointer = Module._denigma_result_gap_report_data(result);
  const size = Module._denigma_result_gap_report_size(result);
  if (!pointer || !size) throw new Error('MNX conversion returned no gap report.');
  return JSON.parse(new TextDecoder().decode(Module.HEAPU8.slice(pointer, pointer + size)));
}

function assertPageMetrics(label, width, height, spatium, hasMargins) {
  if (width <= 0 || height <= 0 || spatium <= 0) throw new Error(`${label} returned no page metrics`);
  if (hasMargins !== 1) throw new Error(`${label} returned no page margins`);
}

function inspect(dataPointer, size, namePointer, label) {
  const inspection = Module._denigma_inspect(dataPointer, size, namePointer);
  try {
    if (!Module._denigma_result_success(inspection)) throw new Error(`${label} failed:\n${messages(inspection)}`);
    const scoreName = Module.UTF8ToString(Module._denigma_result_score_name(inspection));
    if (!scoreName) throw new Error(`${label} returned no score name or fallback`);
    assertPageMetrics(`${label} score`,
      Module._denigma_result_score_page_width_mm(inspection),
      Module._denigma_result_score_page_height_mm(inspection),
      Module._denigma_result_score_spatium_mm(inspection),
      Module._denigma_result_score_has_page_margins(inspection));
    const margins = [
      Module._denigma_result_score_page_margin_top_sp(inspection),
      Module._denigma_result_score_page_margin_bottom_sp(inspection),
      Module._denigma_result_score_page_margin_left_sp(inspection),
      Module._denigma_result_score_page_margin_right_sp(inspection)
    ];
    if (margins.some((value) => !Number.isFinite(value) || value < 0)) {
      throw new Error(`${label} returned invalid score page margins: ${margins}`);
    }
    const partCount = Module._denigma_result_part_count(inspection);
    const parts = Array.from({ length: partCount }, (_, index) => {
      assertPageMetrics(`${label} part ${index}`,
        Module._denigma_result_part_page_width_mm(inspection, index),
        Module._denigma_result_part_page_height_mm(inspection, index),
        Module._denigma_result_part_spatium_mm(inspection, index),
        Module._denigma_result_part_has_page_margins(inspection, index));
      return {
        id: Module._denigma_result_part_id(inspection, index),
        name: Module.UTF8ToString(Module._denigma_result_part_name(inspection, index)),
        outputIndex: Module._denigma_result_part_output_index(inspection, index)
      };
    });
    console.log(`${label}: ${scoreName}, ${partCount} linked parts`);
    return parts;
  } finally {
    Module._denigma_result_destroy(inspection);
  }
}

// Builds a zip archive with one stored (uncompressed) entry, which is all the
// zipped-EnigmaXML input path needs.
function storedZip(name, data) {
  const crcTable = Uint32Array.from({ length: 256 }, (_, index) => {
    let value = index;
    for (let bit = 0; bit < 8; bit += 1) value = value & 1 ? 0xedb88320 ^ (value >>> 1) : value >>> 1;
    return value >>> 0;
  });
  let crc = 0xffffffff;
  for (const byte of data) crc = crcTable[(crc ^ byte) & 0xff] ^ (crc >>> 8);
  crc = (crc ^ 0xffffffff) >>> 0;

  const nameBytes = new TextEncoder().encode(name);
  const localHeaderSize = 30;
  const centralHeaderSize = 46;
  const endRecordSize = 22;
  const bytes = new Uint8Array(localHeaderSize + nameBytes.length + data.length + centralHeaderSize + nameBytes.length + endRecordSize);
  const view = new DataView(bytes.buffer);
  let offset = 0;
  const write32 = (value) => { view.setUint32(offset, value, true); offset += 4; };
  const write16 = (value) => { view.setUint16(offset, value, true); offset += 2; };
  const writeBytes = (value) => { bytes.set(value, offset); offset += value.length; };
  const writeEntryFields = () => {
    write16(20); write16(0); write16(0); write16(0); write16(0);
    write32(crc); write32(data.length); write32(data.length); write16(nameBytes.length); write16(0);
  };

  const localOffset = offset;
  write32(0x04034b50); writeEntryFields(); writeBytes(nameBytes); writeBytes(data);
  const centralOffset = offset;
  write32(0x02014b50); write16(20); writeEntryFields();
  write16(0); write16(0); write16(0); write32(0); write32(localOffset); writeBytes(nameBytes);
  const centralSize = offset - centralOffset;
  write32(0x06054b50); write16(0); write16(0); write16(1); write16(1); write32(centralSize); write32(centralOffset); write16(0);
  return bytes;
}

function exerciseEnigmaXmlInput(bytes, name, label) {
  withInput(bytes, name, (dataPointer, namePointer) => {
    inspect(dataPointer, bytes.byteLength, namePointer, `${label} inspection`);
    assertResult(convert(dataPointer, bytes.byteLength, namePointer, FORMAT_MUSICXML, { selection: [0] }),
      `${label} to MusicXML`, '<score-partwise', { indices: [0] });
    assertResult(convert(dataPointer, bytes.byteLength, namePointer, FORMAT_MNX), `${label} to MNX`, '"mnx"');
    assertResult(convert(dataPointer, bytes.byteLength, namePointer, FORMAT_ENIGMAXML), `${label} pass-through`, '<finale');
  });
}

const input = await readFile(resolve(musxArg));
withInput(input, 'sample.musx', (dataPointer, namePointer) => {
  const size = input.byteLength;
  const parts = inspect(dataPointer, size, namePointer, 'MUSX inspection');

  assertResult(convert(dataPointer, size, namePointer, FORMAT_MUSICXML, { allFonts: 1, selection: [0] }),
    'MusicXML with all source fonts available', '<score-partwise', { indices: [0] });
  if (parts.length) {
    const partIndex = parts[0].outputIndex;
    assertResult(convert(dataPointer, size, namePointer, FORMAT_MUSICXML, { includeTempo: 1, selection: [partIndex] }),
      'MusicXML linked part', '<score-partwise', { indices: [partIndex] });
    assertResult(convert(dataPointer, size, namePointer, FORMAT_MUSICXML, { selection: [0, partIndex] }),
      'MusicXML score and linked part', '<score-partwise', { outputCount: 2, indices: [0, partIndex] });
  }
  assertResult(convert(dataPointer, size, namePointer, FORMAT_MNX), 'MNX with verbose logging', '"mnx"', { verbose: true });
  const enigmaXml = assertResult(convert(dataPointer, size, namePointer, FORMAT_ENIGMAXML), 'EnigmaXML', '<finale');

  exerciseEnigmaXmlInput(enigmaXml, 'sample.enigmaxml', 'EnigmaXML input');
  exerciseEnigmaXmlInput(storedZip('sample.enigmaxml', enigmaXml), 'sample.enigmaxml.zip', 'Zipped EnigmaXML input');

  withInput(new Uint8Array([1, 2, 3]), 'invalid.musx', (invalidPointer, invalidNamePointer) => {
    const invalid = Module._denigma_inspect(invalidPointer, 3, invalidNamePointer);
    try {
      if (Module._denigma_result_success(invalid)) throw new Error('Invalid MUSX input unexpectedly succeeded');
      if (!messages(invalid)) throw new Error('Invalid MUSX input returned no diagnostic');
      console.log('Invalid MUSX: rejected with diagnostics');
    } finally {
      Module._denigma_result_destroy(invalid);
    }
  });
});

const chordInput = await readFile(chordSample);
withInput(chordInput, 'chords.musx', (dataPointer, namePointer) => {
  const result = convert(dataPointer, chordInput.byteLength, namePointer, FORMAT_MNX);
  try {
    if (!Module._denigma_result_success(result)) throw new Error(`Chord MNX conversion failed:\n${messages(result)}`);
    const report = gapReport(result);
    if (report.schemaVersion !== 1 || report.targetFormat !== 'mnx') {
      throw new Error('Chord gap report has invalid envelope metadata.');
    }
    if (!report.source.document.includes('<finale') || !report.source.document.includes('<chordAssign')) {
      throw new Error('Chord gap report does not retain the EnigmaXML source evidence.');
    }
    const chordGaps = report.gaps.filter((gap) => gap.code === 'finale.chord-symbol');
    if (!chordGaps.length || chordGaps.some((gap) => gap.payloadVersion !== 1
      || gap.target.format !== 'mnx'
      || gap.target.representation !== 'none'
      || gap.target.cause !== 'target-unsupported'
      || gap.source.recordType !== 'chordAssign')) {
      throw new Error('Chord gap report does not contain source-located chord assignments.');
    }
    console.log(`MNX gap report: ${chordGaps.length} chord symbol gaps.`);
  } finally {
    Module._denigma_result_destroy(result);
  }
});
