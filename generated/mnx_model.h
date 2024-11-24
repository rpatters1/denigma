//  To parse this JSON data, first install
//
//      json.hpp  https://github.com/nlohmann/json
//
//  Then include this file, and then do
//
//     MnxModel data = nlohmann::json::parse(jsonString);

#pragma once

#include <optional>
#include <variant>
#include "json.hpp"

#include <optional>
#include <stdexcept>
#include <regex>
#include <unordered_map>

#ifndef NLOHMANN_OPT_HELPER
#define NLOHMANN_OPT_HELPER
namespace nlohmann {
    template <typename T>
    struct adl_serializer<std::shared_ptr<T>> {
        static void to_json(json & j, const std::shared_ptr<T> & opt) {
            if (!opt) j = nullptr; else j = *opt;
        }

        static std::shared_ptr<T> from_json(const json & j) {
            if (j.is_null()) return std::make_shared<T>(); else return std::make_shared<T>(j.get<T>());
        }
    };
    template <typename T>
    struct adl_serializer<std::optional<T>> {
        static void to_json(json & j, const std::optional<T> & opt) {
            if (!opt) j = nullptr; else j = *opt;
        }

        static std::optional<T> from_json(const json & j) {
            if (j.is_null()) return std::make_optional<T>(); else return std::make_optional<T>(j.get<T>());
        }
    };
}
#endif

namespace mnx {
    using nlohmann::json;

    #ifndef NLOHMANN_UNTYPED_mnx_HELPER
    #define NLOHMANN_UNTYPED_mnx_HELPER
    inline json get_untyped(const json & j, const char * property) {
        if (j.find(property) != j.end()) {
            return j.at(property).get<json>();
        }
        return json();
    }

    inline json get_untyped(const json & j, std::string property) {
        return get_untyped(j, property.data());
    }
    #endif

    #ifndef NLOHMANN_OPTIONAL_mnx_HELPER
    #define NLOHMANN_OPTIONAL_mnx_HELPER
    template <typename T>
    inline std::shared_ptr<T> get_heap_optional(const json & j, const char * property) {
        auto it = j.find(property);
        if (it != j.end() && !it->is_null()) {
            return j.at(property).get<std::shared_ptr<T>>();
        }
        return std::shared_ptr<T>();
    }

    template <typename T>
    inline std::shared_ptr<T> get_heap_optional(const json & j, std::string property) {
        return get_heap_optional<T>(j, property.data());
    }
    template <typename T>
    inline std::optional<T> get_stack_optional(const json & j, const char * property) {
        auto it = j.find(property);
        if (it != j.end() && !it->is_null()) {
            return j.at(property).get<std::optional<T>>();
        }
        return std::optional<T>();
    }

    template <typename T>
    inline std::optional<T> get_stack_optional(const json & j, std::string property) {
        return get_stack_optional<T>(j, property.data());
    }
    #endif

    enum class BarlineType : int { DASHED, DOTTED, DOUBLE, FINAL, HEAVY, HEAVY_HEAVY, HEAVY_LIGHT, NO_BARLINE, REGULAR, SHORT, TICK };

    class Barline {
        public:
        Barline() = default;
        virtual ~Barline() = default;

        private:
        BarlineType type;

        public:
        const BarlineType & get_type() const { return type; }
        BarlineType & get_mutable_type() { return type; }
        void set_type(const BarlineType & value) { this->type = value; }
    };

    class Ending {
        public:
        Ending() = default;
        virtual ~Ending() = default;

        private:
        std::optional<std::string> ending_class;
        std::optional<std::string> color;
        int64_t duration;
        std::optional<std::vector<int64_t>> numbers;
        std::optional<bool> open;

        public:
        std::optional<std::string> get_ending_class() const { return ending_class; }
        void set_ending_class(std::optional<std::string> value) { this->ending_class = value; }

        std::optional<std::string> get_color() const { return color; }
        void set_color(std::optional<std::string> value) { this->color = value; }

        const int64_t & get_duration() const { return duration; }
        int64_t & get_mutable_duration() { return duration; }
        void set_duration(const int64_t & value) { this->duration = value; }

        std::optional<std::vector<int64_t>> get_numbers() const { return numbers; }
        void set_numbers(std::optional<std::vector<int64_t>> value) { this->numbers = value; }

        std::optional<bool> get_open() const { return open; }
        void set_open(std::optional<bool> value) { this->open = value; }
    };

    class PositionClass {
        public:
        PositionClass() = default;
        virtual ~PositionClass() = default;

        private:
        std::vector<int64_t> fraction;
        std::optional<int64_t> grace_index;

        public:
        const std::vector<int64_t> & get_fraction() const { return fraction; }
        std::vector<int64_t> & get_mutable_fraction() { return fraction; }
        void set_fraction(const std::vector<int64_t> & value) { this->fraction = value; }

        std::optional<int64_t> get_grace_index() const { return grace_index; }
        void set_grace_index(std::optional<int64_t> value) { this->grace_index = value; }
    };

    class Fine {
        public:
        Fine() = default;
        virtual ~Fine() = default;

        private:
        std::optional<std::string> fine_class;
        std::optional<std::string> color;
        PositionClass location;

        public:
        std::optional<std::string> get_fine_class() const { return fine_class; }
        void set_fine_class(std::optional<std::string> value) { this->fine_class = value; }

        std::optional<std::string> get_color() const { return color; }
        void set_color(std::optional<std::string> value) { this->color = value; }

        const PositionClass & get_location() const { return location; }
        PositionClass & get_mutable_location() { return location; }
        void set_location(const PositionClass & value) { this->location = value; }
    };

    enum class JumpType : int { DSALFINE, SEGNO };

    class Jump {
        public:
        Jump() = default;
        virtual ~Jump() = default;

        private:
        PositionClass location;
        JumpType type;

        public:
        const PositionClass & get_location() const { return location; }
        PositionClass & get_mutable_location() { return location; }
        void set_location(const PositionClass & value) { this->location = value; }

        const JumpType & get_type() const { return type; }
        JumpType & get_mutable_type() { return type; }
        void set_type(const JumpType & value) { this->type = value; }
    };

    class Key {
        public:
        Key() = default;
        virtual ~Key() = default;

        private:
        std::optional<std::string> key_class;
        std::optional<std::string> color;
        int64_t fifths;

        public:
        std::optional<std::string> get_key_class() const { return key_class; }
        void set_key_class(std::optional<std::string> value) { this->key_class = value; }

        std::optional<std::string> get_color() const { return color; }
        void set_color(std::optional<std::string> value) { this->color = value; }

        const int64_t & get_fifths() const { return fifths; }
        int64_t & get_mutable_fifths() { return fifths; }
        void set_fifths(const int64_t & value) { this->fifths = value; }
    };

    class RepeatEnd {
        public:
        RepeatEnd() = default;
        virtual ~RepeatEnd() = default;

        private:
        std::optional<int64_t> times;

        public:
        std::optional<int64_t> get_times() const { return times; }
        void set_times(std::optional<int64_t> value) { this->times = value; }
    };

    class RepeatStart {
        public:
        RepeatStart() = default;
        virtual ~RepeatStart() = default;

        private:

        public:
    };

    class Segno {
        public:
        Segno() = default;
        virtual ~Segno() = default;

        private:
        std::optional<std::string> segno_class;
        std::optional<std::string> color;
        std::optional<std::string> glyph;
        PositionClass location;

        public:
        std::optional<std::string> get_segno_class() const { return segno_class; }
        void set_segno_class(std::optional<std::string> value) { this->segno_class = value; }

        std::optional<std::string> get_color() const { return color; }
        void set_color(std::optional<std::string> value) { this->color = value; }

        std::optional<std::string> get_glyph() const { return glyph; }
        void set_glyph(std::optional<std::string> value) { this->glyph = value; }

        const PositionClass & get_location() const { return location; }
        PositionClass & get_mutable_location() { return location; }
        void set_location(const PositionClass & value) { this->location = value; }
    };

    class EndClass {
        public:
        EndClass() = default;
        virtual ~EndClass() = default;

        private:
        int64_t bar;
        PositionClass position;

        public:
        const int64_t & get_bar() const { return bar; }
        int64_t & get_mutable_bar() { return bar; }
        void set_bar(const int64_t & value) { this->bar = value; }

        const PositionClass & get_position() const { return position; }
        PositionClass & get_mutable_position() { return position; }
        void set_position(const PositionClass & value) { this->position = value; }
    };

    enum class Base : int { BREVE, DUPLEX_MAXIMA, EIGHTH, HALF, LONGA, MAXIMA, QUARTER, THE_1024_TH, THE_128_TH, THE_16_TH, THE_2048_TH, THE_256_TH, THE_32_ND, THE_4096_TH, THE_512_TH, THE_64_TH, WHOLE };

    class ValueClass {
        public:
        ValueClass() = default;
        virtual ~ValueClass() = default;

        private:
        Base base;
        std::optional<int64_t> dots;

        public:
        const Base & get_base() const { return base; }
        Base & get_mutable_base() { return base; }
        void set_base(const Base & value) { this->base = value; }

        std::optional<int64_t> get_dots() const { return dots; }
        void set_dots(std::optional<int64_t> value) { this->dots = value; }
    };

    class Tempo {
        public:
        Tempo() = default;
        virtual ~Tempo() = default;

        private:
        int64_t bpm;
        std::optional<EndClass> location;
        ValueClass value;

        public:
        const int64_t & get_bpm() const { return bpm; }
        int64_t & get_mutable_bpm() { return bpm; }
        void set_bpm(const int64_t & value) { this->bpm = value; }

        std::optional<EndClass> get_location() const { return location; }
        void set_location(std::optional<EndClass> value) { this->location = value; }

        const ValueClass & get_value() const { return value; }
        ValueClass & get_mutable_value() { return value; }
        void set_value(const ValueClass & value) { this->value = value; }
    };

    class Time {
        public:
        Time() = default;
        virtual ~Time() = default;

        private:
        int64_t count;
        int64_t unit;

        public:
        const int64_t & get_count() const { return count; }
        int64_t & get_mutable_count() { return count; }
        void set_count(const int64_t & value) { this->count = value; }

        const int64_t & get_unit() const { return unit; }
        int64_t & get_mutable_unit() { return unit; }
        void set_unit(const int64_t & value) { this->unit = value; }
    };

    class GlobalMeasure {
        public:
        GlobalMeasure() = default;
        virtual ~GlobalMeasure() = default;

        private:
        std::optional<Barline> barline;
        std::optional<Ending> ending;
        std::optional<Fine> fine;
        std::optional<int64_t> index;
        std::optional<Jump> jump;
        std::optional<Key> key;
        std::optional<int64_t> number;
        std::optional<RepeatEnd> repeat_end;
        std::optional<RepeatStart> repeat_start;
        std::optional<Segno> segno;
        std::optional<std::vector<Tempo>> tempos;
        std::optional<Time> time;

        public:
        std::optional<Barline> get_barline() const { return barline; }
        void set_barline(std::optional<Barline> value) { this->barline = value; }

        std::optional<Ending> get_ending() const { return ending; }
        void set_ending(std::optional<Ending> value) { this->ending = value; }

        std::optional<Fine> get_fine() const { return fine; }
        void set_fine(std::optional<Fine> value) { this->fine = value; }

        std::optional<int64_t> get_index() const { return index; }
        void set_index(std::optional<int64_t> value) { this->index = value; }

        std::optional<Jump> get_jump() const { return jump; }
        void set_jump(std::optional<Jump> value) { this->jump = value; }

        std::optional<Key> get_key() const { return key; }
        void set_key(std::optional<Key> value) { this->key = value; }

        std::optional<int64_t> get_number() const { return number; }
        void set_number(std::optional<int64_t> value) { this->number = value; }

        std::optional<RepeatEnd> get_repeat_end() const { return repeat_end; }
        void set_repeat_end(std::optional<RepeatEnd> value) { this->repeat_end = value; }

        std::optional<RepeatStart> get_repeat_start() const { return repeat_start; }
        void set_repeat_start(std::optional<RepeatStart> value) { this->repeat_start = value; }

        std::optional<Segno> get_segno() const { return segno; }
        void set_segno(std::optional<Segno> value) { this->segno = value; }

        std::optional<std::vector<Tempo>> get_tempos() const { return tempos; }
        void set_tempos(std::optional<std::vector<Tempo>> value) { this->tempos = value; }

        std::optional<Time> get_time() const { return time; }
        void set_time(std::optional<Time> value) { this->time = value; }
    };

    class Style {
        public:
        Style() = default;
        virtual ~Style() = default;

        private:
        std::optional<std::string> color;
        std::string selector;

        public:
        std::optional<std::string> get_color() const { return color; }
        void set_color(std::optional<std::string> value) { this->color = value; }

        const std::string & get_selector() const { return selector; }
        std::string & get_mutable_selector() { return selector; }
        void set_selector(const std::string & value) { this->selector = value; }
    };

    class Global {
        public:
        Global() = default;
        virtual ~Global() = default;

        private:
        std::vector<GlobalMeasure> measures;
        std::optional<std::vector<Style>> styles;

        public:
        const std::vector<GlobalMeasure> & get_measures() const { return measures; }
        std::vector<GlobalMeasure> & get_mutable_measures() { return measures; }
        void set_measures(const std::vector<GlobalMeasure> & value) { this->measures = value; }

        std::optional<std::vector<Style>> get_styles() const { return styles; }
        void set_styles(std::optional<std::vector<Style>> value) { this->styles = value; }
    };

    enum class Stem : int { DOWN, UP };

    class Source {
        public:
        Source() = default;
        virtual ~Source() = default;

        private:
        std::optional<std::string> label;
        std::optional<std::string> labelref;
        std::string part;
        std::optional<int64_t> staff;
        std::optional<Stem> stem;
        std::optional<std::string> voice;

        public:
        std::optional<std::string> get_label() const { return label; }
        void set_label(std::optional<std::string> value) { this->label = value; }

        std::optional<std::string> get_labelref() const { return labelref; }
        void set_labelref(std::optional<std::string> value) { this->labelref = value; }

        const std::string & get_part() const { return part; }
        std::string & get_mutable_part() { return part; }
        void set_part(const std::string & value) { this->part = value; }

        std::optional<int64_t> get_staff() const { return staff; }
        void set_staff(std::optional<int64_t> value) { this->staff = value; }

        std::optional<Stem> get_stem() const { return stem; }
        void set_stem(std::optional<Stem> value) { this->stem = value; }

        std::optional<std::string> get_voice() const { return voice; }
        void set_voice(std::optional<std::string> value) { this->voice = value; }
    };

    enum class ContentSymbol : int { BRACE, BRACKET, NO_SYMBOL };

    enum class PurpleType : int { GROUP, STAFF };

    class LayoutContent {
        public:
        LayoutContent() = default;
        virtual ~LayoutContent() = default;

        private:
        std::optional<std::vector<LayoutContent>> content;
        std::optional<std::string> label;
        std::optional<ContentSymbol> symbol;
        PurpleType type;
        std::optional<std::string> labelref;
        std::optional<std::vector<Source>> sources;

        public:
        std::optional<std::vector<LayoutContent>> get_content() const { return content; }
        void set_content(std::optional<std::vector<LayoutContent>> value) { this->content = value; }

        std::optional<std::string> get_label() const { return label; }
        void set_label(std::optional<std::string> value) { this->label = value; }

        std::optional<ContentSymbol> get_symbol() const { return symbol; }
        void set_symbol(std::optional<ContentSymbol> value) { this->symbol = value; }

        const PurpleType & get_type() const { return type; }
        PurpleType & get_mutable_type() { return type; }
        void set_type(const PurpleType & value) { this->type = value; }

        std::optional<std::string> get_labelref() const { return labelref; }
        void set_labelref(std::optional<std::string> value) { this->labelref = value; }

        std::optional<std::vector<Source>> get_sources() const { return sources; }
        void set_sources(std::optional<std::vector<Source>> value) { this->sources = value; }
    };

    class Layout {
        public:
        Layout() = default;
        virtual ~Layout() = default;

        private:
        std::vector<LayoutContent> content;
        std::string id;

        public:
        const std::vector<LayoutContent> & get_content() const { return content; }
        std::vector<LayoutContent> & get_mutable_content() { return content; }
        void set_content(const std::vector<LayoutContent> & value) { this->content = value; }

        const std::string & get_id() const { return id; }
        std::string & get_mutable_id() { return id; }
        void set_id(const std::string & value) { this->id = value; }
    };

    class Support {
        public:
        Support() = default;
        virtual ~Support() = default;

        private:
        std::optional<bool> use_accidental_display;

        public:
        std::optional<bool> get_use_accidental_display() const { return use_accidental_display; }
        void set_use_accidental_display(std::optional<bool> value) { this->use_accidental_display = value; }
    };

    class Mnx {
        public:
        Mnx() = default;
        virtual ~Mnx() = default;

        private:
        std::optional<Support> support;
        int64_t version;

        public:
        std::optional<Support> get_support() const { return support; }
        void set_support(std::optional<Support> value) { this->support = value; }

        const int64_t & get_version() const { return version; }
        int64_t & get_mutable_version() { return version; }
        void set_version(const int64_t & value) { this->version = value; }
    };

    enum class Direction : int { LEFT, RIGHT };

    class Hook {
        public:
        Hook() = default;
        virtual ~Hook() = default;

        private:
        Direction direction;
        std::string event;

        public:
        const Direction & get_direction() const { return direction; }
        Direction & get_mutable_direction() { return direction; }
        void set_direction(const Direction & value) { this->direction = value; }

        const std::string & get_event() const { return event; }
        std::string & get_mutable_event() { return event; }
        void set_event(const std::string & value) { this->event = value; }
    };

    class BeamElement {
        public:
        BeamElement() = default;
        virtual ~BeamElement() = default;

        private:
        std::vector<std::string> events;
        std::optional<std::vector<Hook>> hooks;
        std::optional<std::vector<BeamElement>> inner;

        public:
        const std::vector<std::string> & get_events() const { return events; }
        std::vector<std::string> & get_mutable_events() { return events; }
        void set_events(const std::vector<std::string> & value) { this->events = value; }

        std::optional<std::vector<Hook>> get_hooks() const { return hooks; }
        void set_hooks(std::optional<std::vector<Hook>> value) { this->hooks = value; }

        std::optional<std::vector<BeamElement>> get_inner() const { return inner; }
        void set_inner(std::optional<std::vector<BeamElement>> value) { this->inner = value; }
    };

    enum class Sign : int { C, F, G };

    class ClefClef {
        public:
        ClefClef() = default;
        virtual ~ClefClef() = default;

        private:
        std::optional<std::string> clef_class;
        std::optional<std::string> color;
        std::optional<std::string> glyph;
        std::optional<int64_t> octave;
        Sign sign;
        int64_t staff_position;

        public:
        std::optional<std::string> get_clef_class() const { return clef_class; }
        void set_clef_class(std::optional<std::string> value) { this->clef_class = value; }

        std::optional<std::string> get_color() const { return color; }
        void set_color(std::optional<std::string> value) { this->color = value; }

        std::optional<std::string> get_glyph() const { return glyph; }
        void set_glyph(std::optional<std::string> value) { this->glyph = value; }

        std::optional<int64_t> get_octave() const { return octave; }
        void set_octave(std::optional<int64_t> value) { this->octave = value; }

        const Sign & get_sign() const { return sign; }
        Sign & get_mutable_sign() { return sign; }
        void set_sign(const Sign & value) { this->sign = value; }

        const int64_t & get_staff_position() const { return staff_position; }
        int64_t & get_mutable_staff_position() { return staff_position; }
        void set_staff_position(const int64_t & value) { this->staff_position = value; }
    };

    class ClefElement {
        public:
        ClefElement() = default;
        virtual ~ClefElement() = default;

        private:
        ClefClef clef;
        std::optional<PositionClass> position;
        std::optional<int64_t> staff;

        public:
        const ClefClef & get_clef() const { return clef; }
        ClefClef & get_mutable_clef() { return clef; }
        void set_clef(const ClefClef & value) { this->clef = value; }

        std::optional<PositionClass> get_position() const { return position; }
        void set_position(std::optional<PositionClass> value) { this->position = value; }

        std::optional<int64_t> get_staff() const { return staff; }
        void set_staff(std::optional<int64_t> value) { this->staff = value; }
    };

    enum class Bracket : int { AUTO, NO, YES };

    class Lines {
        public:
        Lines() = default;
        virtual ~Lines() = default;

        private:

        public:
    };

    class Lyrics {
        public:
        Lyrics() = default;
        virtual ~Lyrics() = default;

        private:
        std::optional<Lines> lines;

        public:
        std::optional<Lines> get_lines() const { return lines; }
        void set_lines(std::optional<Lines> value) { this->lines = value; }
    };

    class Accent {
        public:
        Accent() = default;
        virtual ~Accent() = default;

        private:
        std::optional<Stem> pointing;

        public:
        std::optional<Stem> get_pointing() const { return pointing; }
        void set_pointing(std::optional<Stem> value) { this->pointing = value; }
    };

    class Breath {
        public:
        Breath() = default;
        virtual ~Breath() = default;

        private:
        std::optional<std::string> symbol;

        public:
        std::optional<std::string> get_symbol() const { return symbol; }
        void set_symbol(std::optional<std::string> value) { this->symbol = value; }
    };

    class SoftAccent {
        public:
        SoftAccent() = default;
        virtual ~SoftAccent() = default;

        private:

        public:
    };

    class Spiccato {
        public:
        Spiccato() = default;
        virtual ~Spiccato() = default;

        private:

        public:
    };

    class Staccatissimo {
        public:
        Staccatissimo() = default;
        virtual ~Staccatissimo() = default;

        private:

        public:
    };

    class Staccato {
        public:
        Staccato() = default;
        virtual ~Staccato() = default;

        private:

        public:
    };

    class Stress {
        public:
        Stress() = default;
        virtual ~Stress() = default;

        private:

        public:
    };

    class StrongAccent {
        public:
        StrongAccent() = default;
        virtual ~StrongAccent() = default;

        private:
        std::optional<Stem> pointing;

        public:
        std::optional<Stem> get_pointing() const { return pointing; }
        void set_pointing(std::optional<Stem> value) { this->pointing = value; }
    };

    class Tenuto {
        public:
        Tenuto() = default;
        virtual ~Tenuto() = default;

        private:

        public:
    };

    class Tremolo {
        public:
        Tremolo() = default;
        virtual ~Tremolo() = default;

        private:
        int64_t marks;

        public:
        const int64_t & get_marks() const { return marks; }
        int64_t & get_mutable_marks() { return marks; }
        void set_marks(const int64_t & value) { this->marks = value; }
    };

    class Unstress {
        public:
        Unstress() = default;
        virtual ~Unstress() = default;

        private:

        public:
    };

    class Markings {
        public:
        Markings() = default;
        virtual ~Markings() = default;

        private:
        std::optional<Accent> accent;
        std::optional<Breath> breath;
        std::optional<SoftAccent> soft_accent;
        std::optional<Spiccato> spiccato;
        std::optional<Staccatissimo> staccatissimo;
        std::optional<Staccato> staccato;
        std::optional<Stress> stress;
        std::optional<StrongAccent> strong_accent;
        std::optional<Tenuto> tenuto;
        std::optional<Tremolo> tremolo;
        std::optional<Unstress> unstress;

        public:
        std::optional<Accent> get_accent() const { return accent; }
        void set_accent(std::optional<Accent> value) { this->accent = value; }

        std::optional<Breath> get_breath() const { return breath; }
        void set_breath(std::optional<Breath> value) { this->breath = value; }

        std::optional<SoftAccent> get_soft_accent() const { return soft_accent; }
        void set_soft_accent(std::optional<SoftAccent> value) { this->soft_accent = value; }

        std::optional<Spiccato> get_spiccato() const { return spiccato; }
        void set_spiccato(std::optional<Spiccato> value) { this->spiccato = value; }

        std::optional<Staccatissimo> get_staccatissimo() const { return staccatissimo; }
        void set_staccatissimo(std::optional<Staccatissimo> value) { this->staccatissimo = value; }

        std::optional<Staccato> get_staccato() const { return staccato; }
        void set_staccato(std::optional<Staccato> value) { this->staccato = value; }

        std::optional<Stress> get_stress() const { return stress; }
        void set_stress(std::optional<Stress> value) { this->stress = value; }

        std::optional<StrongAccent> get_strong_accent() const { return strong_accent; }
        void set_strong_accent(std::optional<StrongAccent> value) { this->strong_accent = value; }

        std::optional<Tenuto> get_tenuto() const { return tenuto; }
        void set_tenuto(std::optional<Tenuto> value) { this->tenuto = value; }

        std::optional<Tremolo> get_tremolo() const { return tremolo; }
        void set_tremolo(std::optional<Tremolo> value) { this->tremolo = value; }

        std::optional<Unstress> get_unstress() const { return unstress; }
        void set_unstress(std::optional<Unstress> value) { this->unstress = value; }
    };

    enum class EnclosureSymbol : int { BRACKETS, PARENTHESES };

    class Enclosure {
        public:
        Enclosure() = default;
        virtual ~Enclosure() = default;

        private:
        EnclosureSymbol symbol;

        public:
        const EnclosureSymbol & get_symbol() const { return symbol; }
        EnclosureSymbol & get_mutable_symbol() { return symbol; }
        void set_symbol(const EnclosureSymbol & value) { this->symbol = value; }
    };

    class AccidentalDisplay {
        public:
        AccidentalDisplay() = default;
        virtual ~AccidentalDisplay() = default;

        private:
        std::optional<Enclosure> enclosure;
        bool show;

        public:
        std::optional<Enclosure> get_enclosure() const { return enclosure; }
        void set_enclosure(std::optional<Enclosure> value) { this->enclosure = value; }

        const bool & get_show() const { return show; }
        bool & get_mutable_show() { return show; }
        void set_show(const bool & value) { this->show = value; }
    };

    class Perform {
        public:
        Perform() = default;
        virtual ~Perform() = default;

        private:

        public:
    };

    enum class Step : int { A, B, C, D, E, F, G };

    class Pitch {
        public:
        Pitch() = default;
        virtual ~Pitch() = default;

        private:
        std::optional<int64_t> alter;
        int64_t octave;
        Step step;

        public:
        std::optional<int64_t> get_alter() const { return alter; }
        void set_alter(std::optional<int64_t> value) { this->alter = value; }

        const int64_t & get_octave() const { return octave; }
        int64_t & get_mutable_octave() { return octave; }
        void set_octave(const int64_t & value) { this->octave = value; }

        const Step & get_step() const { return step; }
        Step & get_mutable_step() { return step; }
        void set_step(const Step & value) { this->step = value; }
    };

    class Tie {
        public:
        Tie() = default;
        virtual ~Tie() = default;

        private:
        std::optional<std::string> location;
        std::optional<Stem> side;
        std::optional<std::string> target;

        public:
        std::optional<std::string> get_location() const { return location; }
        void set_location(std::optional<std::string> value) { this->location = value; }

        std::optional<Stem> get_side() const { return side; }
        void set_side(std::optional<Stem> value) { this->side = value; }

        std::optional<std::string> get_target() const { return target; }
        void set_target(std::optional<std::string> value) { this->target = value; }
    };

    class Note {
        public:
        Note() = default;
        virtual ~Note() = default;

        private:
        std::optional<AccidentalDisplay> accidental_display;
        std::optional<std::string> note_class;
        std::optional<std::string> id;
        std::optional<Perform> perform;
        Pitch pitch;
        std::optional<std::string> smufl_font;
        std::optional<int64_t> staff;
        std::optional<Tie> tie;

        public:
        std::optional<AccidentalDisplay> get_accidental_display() const { return accidental_display; }
        void set_accidental_display(std::optional<AccidentalDisplay> value) { this->accidental_display = value; }

        std::optional<std::string> get_note_class() const { return note_class; }
        void set_note_class(std::optional<std::string> value) { this->note_class = value; }

        std::optional<std::string> get_id() const { return id; }
        void set_id(std::optional<std::string> value) { this->id = value; }

        std::optional<Perform> get_perform() const { return perform; }
        void set_perform(std::optional<Perform> value) { this->perform = value; }

        const Pitch & get_pitch() const { return pitch; }
        Pitch & get_mutable_pitch() { return pitch; }
        void set_pitch(const Pitch & value) { this->pitch = value; }

        std::optional<std::string> get_smufl_font() const { return smufl_font; }
        void set_smufl_font(std::optional<std::string> value) { this->smufl_font = value; }

        std::optional<int64_t> get_staff() const { return staff; }
        void set_staff(std::optional<int64_t> value) { this->staff = value; }

        std::optional<Tie> get_tie() const { return tie; }
        void set_tie(std::optional<Tie> value) { this->tie = value; }
    };

    class Rest {
        public:
        Rest() = default;
        virtual ~Rest() = default;

        private:
        std::optional<int64_t> staff_position;

        public:
        std::optional<int64_t> get_staff_position() const { return staff_position; }
        void set_staff_position(std::optional<int64_t> value) { this->staff_position = value; }
    };

    class Slur {
        public:
        Slur() = default;
        virtual ~Slur() = default;

        private:
        std::optional<std::string> end_note;
        std::optional<std::string> line_type;
        std::optional<std::string> location;
        std::optional<Stem> side;
        std::optional<Stem> side_end;
        std::optional<std::string> start_note;
        std::optional<std::string> target;

        public:
        std::optional<std::string> get_end_note() const { return end_note; }
        void set_end_note(std::optional<std::string> value) { this->end_note = value; }

        std::optional<std::string> get_line_type() const { return line_type; }
        void set_line_type(std::optional<std::string> value) { this->line_type = value; }

        std::optional<std::string> get_location() const { return location; }
        void set_location(std::optional<std::string> value) { this->location = value; }

        std::optional<Stem> get_side() const { return side; }
        void set_side(std::optional<Stem> value) { this->side = value; }

        std::optional<Stem> get_side_end() const { return side_end; }
        void set_side_end(std::optional<Stem> value) { this->side_end = value; }

        std::optional<std::string> get_start_note() const { return start_note; }
        void set_start_note(std::optional<std::string> value) { this->start_note = value; }

        std::optional<std::string> get_target() const { return target; }
        void set_target(std::optional<std::string> value) { this->target = value; }
    };

    enum class FluffyType : int { EVENT };

    class PurpleMnxSchema {
        public:
        PurpleMnxSchema() = default;
        virtual ~PurpleMnxSchema() = default;

        private:
        std::optional<ValueClass> duration;
        std::optional<std::string> id;
        std::optional<Lyrics> lyrics;
        std::optional<Markings> markings;
        std::optional<bool> measure;
        std::optional<std::vector<Note>> notes;
        std::optional<std::string> orient;
        std::optional<Rest> rest;
        std::optional<std::vector<Slur>> slurs;
        std::optional<std::string> smufl_font;
        std::optional<int64_t> staff;
        std::optional<Stem> stem_direction;
        FluffyType type;

        public:
        std::optional<ValueClass> get_duration() const { return duration; }
        void set_duration(std::optional<ValueClass> value) { this->duration = value; }

        std::optional<std::string> get_id() const { return id; }
        void set_id(std::optional<std::string> value) { this->id = value; }

        std::optional<Lyrics> get_lyrics() const { return lyrics; }
        void set_lyrics(std::optional<Lyrics> value) { this->lyrics = value; }

        std::optional<Markings> get_markings() const { return markings; }
        void set_markings(std::optional<Markings> value) { this->markings = value; }

        std::optional<bool> get_measure() const { return measure; }
        void set_measure(std::optional<bool> value) { this->measure = value; }

        std::optional<std::vector<Note>> get_notes() const { return notes; }
        void set_notes(std::optional<std::vector<Note>> value) { this->notes = value; }

        std::optional<std::string> get_orient() const { return orient; }
        void set_orient(std::optional<std::string> value) { this->orient = value; }

        std::optional<Rest> get_rest() const { return rest; }
        void set_rest(std::optional<Rest> value) { this->rest = value; }

        std::optional<std::vector<Slur>> get_slurs() const { return slurs; }
        void set_slurs(std::optional<std::vector<Slur>> value) { this->slurs = value; }

        std::optional<std::string> get_smufl_font() const { return smufl_font; }
        void set_smufl_font(std::optional<std::string> value) { this->smufl_font = value; }

        std::optional<int64_t> get_staff() const { return staff; }
        void set_staff(std::optional<int64_t> value) { this->staff = value; }

        std::optional<Stem> get_stem_direction() const { return stem_direction; }
        void set_stem_direction(std::optional<Stem> value) { this->stem_direction = value; }

        const FluffyType & get_type() const { return type; }
        FluffyType & get_mutable_type() { return type; }
        void set_type(const FluffyType & value) { this->type = value; }
    };

    class PurpleValue {
        public:
        PurpleValue() = default;
        virtual ~PurpleValue() = default;

        private:
        std::optional<Base> base;
        std::optional<int64_t> dots;
        std::optional<ValueClass> duration;
        std::optional<int64_t> multiple;

        public:
        std::optional<Base> get_base() const { return base; }
        void set_base(std::optional<Base> value) { this->base = value; }

        std::optional<int64_t> get_dots() const { return dots; }
        void set_dots(std::optional<int64_t> value) { this->dots = value; }

        std::optional<ValueClass> get_duration() const { return duration; }
        void set_duration(std::optional<ValueClass> value) { this->duration = value; }

        std::optional<int64_t> get_multiple() const { return multiple; }
        void set_multiple(std::optional<int64_t> value) { this->multiple = value; }
    };

    enum class GraceType : int { MAKE_TIME, STEAL_FOLLOWING, STEAL_PREVIOUS };

    class Inner {
        public:
        Inner() = default;
        virtual ~Inner() = default;

        private:
        ValueClass duration;
        int64_t multiple;

        public:
        const ValueClass & get_duration() const { return duration; }
        ValueClass & get_mutable_duration() { return duration; }
        void set_duration(const ValueClass & value) { this->duration = value; }

        const int64_t & get_multiple() const { return multiple; }
        int64_t & get_mutable_multiple() { return multiple; }
        void set_multiple(const int64_t & value) { this->multiple = value; }
    };

    enum class ShowNumber : int { BOTH, INNER, NO_NUMBER };

    enum class TentacledType : int { DYNAMIC, EVENT, GRACE, OTTAVA, SPACE, TUPLET };

    using ValueUnion = std::variant<int64_t, std::string>;

    class Content {
        public:
        Content() = default;
        virtual ~Content() = default;

        private:
        std::optional<PurpleValue> duration;
        std::optional<std::string> id;
        std::optional<Lyrics> lyrics;
        std::optional<Markings> markings;
        std::optional<bool> measure;
        std::optional<std::vector<Note>> notes;
        std::optional<std::string> orient;
        std::optional<Rest> rest;
        std::optional<std::vector<Slur>> slurs;
        std::optional<std::string> smufl_font;
        std::optional<int64_t> staff;
        std::optional<Stem> stem_direction;
        TentacledType type;
        std::optional<std::string> content_class;
        std::optional<std::string> color;
        std::optional<std::vector<PurpleMnxSchema>> content;
        std::optional<GraceType> grace_type;
        std::optional<bool> slash;
        std::optional<Bracket> bracket;
        std::optional<Inner> inner;
        std::optional<Inner> outer;
        std::optional<ShowNumber> show_number;
        std::optional<ShowNumber> show_value;
        std::optional<EndClass> end;
        std::optional<ValueUnion> value;
        std::optional<std::string> glyph;

        public:
        std::optional<PurpleValue> get_duration() const { return duration; }
        void set_duration(std::optional<PurpleValue> value) { this->duration = value; }

        std::optional<std::string> get_id() const { return id; }
        void set_id(std::optional<std::string> value) { this->id = value; }

        std::optional<Lyrics> get_lyrics() const { return lyrics; }
        void set_lyrics(std::optional<Lyrics> value) { this->lyrics = value; }

        std::optional<Markings> get_markings() const { return markings; }
        void set_markings(std::optional<Markings> value) { this->markings = value; }

        std::optional<bool> get_measure() const { return measure; }
        void set_measure(std::optional<bool> value) { this->measure = value; }

        std::optional<std::vector<Note>> get_notes() const { return notes; }
        void set_notes(std::optional<std::vector<Note>> value) { this->notes = value; }

        std::optional<std::string> get_orient() const { return orient; }
        void set_orient(std::optional<std::string> value) { this->orient = value; }

        std::optional<Rest> get_rest() const { return rest; }
        void set_rest(std::optional<Rest> value) { this->rest = value; }

        std::optional<std::vector<Slur>> get_slurs() const { return slurs; }
        void set_slurs(std::optional<std::vector<Slur>> value) { this->slurs = value; }

        std::optional<std::string> get_smufl_font() const { return smufl_font; }
        void set_smufl_font(std::optional<std::string> value) { this->smufl_font = value; }

        std::optional<int64_t> get_staff() const { return staff; }
        void set_staff(std::optional<int64_t> value) { this->staff = value; }

        std::optional<Stem> get_stem_direction() const { return stem_direction; }
        void set_stem_direction(std::optional<Stem> value) { this->stem_direction = value; }

        const TentacledType & get_type() const { return type; }
        TentacledType & get_mutable_type() { return type; }
        void set_type(const TentacledType & value) { this->type = value; }

        std::optional<std::string> get_content_class() const { return content_class; }
        void set_content_class(std::optional<std::string> value) { this->content_class = value; }

        std::optional<std::string> get_color() const { return color; }
        void set_color(std::optional<std::string> value) { this->color = value; }

        std::optional<std::vector<PurpleMnxSchema>> get_content() const { return content; }
        void set_content(std::optional<std::vector<PurpleMnxSchema>> value) { this->content = value; }

        std::optional<GraceType> get_grace_type() const { return grace_type; }
        void set_grace_type(std::optional<GraceType> value) { this->grace_type = value; }

        std::optional<bool> get_slash() const { return slash; }
        void set_slash(std::optional<bool> value) { this->slash = value; }

        std::optional<Bracket> get_bracket() const { return bracket; }
        void set_bracket(std::optional<Bracket> value) { this->bracket = value; }

        std::optional<Inner> get_inner() const { return inner; }
        void set_inner(std::optional<Inner> value) { this->inner = value; }

        std::optional<Inner> get_outer() const { return outer; }
        void set_outer(std::optional<Inner> value) { this->outer = value; }

        std::optional<ShowNumber> get_show_number() const { return show_number; }
        void set_show_number(std::optional<ShowNumber> value) { this->show_number = value; }

        std::optional<ShowNumber> get_show_value() const { return show_value; }
        void set_show_value(std::optional<ShowNumber> value) { this->show_value = value; }

        std::optional<EndClass> get_end() const { return end; }
        void set_end(std::optional<EndClass> value) { this->end = value; }

        std::optional<ValueUnion> get_value() const { return value; }
        void set_value(std::optional<ValueUnion> value) { this->value = value; }

        std::optional<std::string> get_glyph() const { return glyph; }
        void set_glyph(std::optional<std::string> value) { this->glyph = value; }
    };

    class Sequence {
        public:
        Sequence() = default;
        virtual ~Sequence() = default;

        private:
        std::vector<Content> content;
        std::optional<std::string> orient;
        std::optional<int64_t> staff;
        std::optional<std::string> voice;

        public:
        const std::vector<Content> & get_content() const { return content; }
        std::vector<Content> & get_mutable_content() { return content; }
        void set_content(const std::vector<Content> & value) { this->content = value; }

        std::optional<std::string> get_orient() const { return orient; }
        void set_orient(std::optional<std::string> value) { this->orient = value; }

        std::optional<int64_t> get_staff() const { return staff; }
        void set_staff(std::optional<int64_t> value) { this->staff = value; }

        std::optional<std::string> get_voice() const { return voice; }
        void set_voice(std::optional<std::string> value) { this->voice = value; }
    };

    class PartMeasure {
        public:
        PartMeasure() = default;
        virtual ~PartMeasure() = default;

        private:
        std::optional<std::vector<BeamElement>> beams;
        std::optional<std::vector<ClefElement>> clefs;
        std::vector<Sequence> sequences;

        public:
        std::optional<std::vector<BeamElement>> get_beams() const { return beams; }
        void set_beams(std::optional<std::vector<BeamElement>> value) { this->beams = value; }

        std::optional<std::vector<ClefElement>> get_clefs() const { return clefs; }
        void set_clefs(std::optional<std::vector<ClefElement>> value) { this->clefs = value; }

        const std::vector<Sequence> & get_sequences() const { return sequences; }
        std::vector<Sequence> & get_mutable_sequences() { return sequences; }
        void set_sequences(const std::vector<Sequence> & value) { this->sequences = value; }
    };

    class Part {
        public:
        Part() = default;
        virtual ~Part() = default;

        private:
        std::optional<std::string> id;
        std::optional<std::vector<PartMeasure>> measures;
        std::optional<std::string> name;
        std::optional<std::string> short_name;
        std::optional<std::string> smufl_font;
        std::optional<int64_t> staves;

        public:
        std::optional<std::string> get_id() const { return id; }
        void set_id(std::optional<std::string> value) { this->id = value; }

        std::optional<std::vector<PartMeasure>> get_measures() const { return measures; }
        void set_measures(std::optional<std::vector<PartMeasure>> value) { this->measures = value; }

        std::optional<std::string> get_name() const { return name; }
        void set_name(std::optional<std::string> value) { this->name = value; }

        std::optional<std::string> get_short_name() const { return short_name; }
        void set_short_name(std::optional<std::string> value) { this->short_name = value; }

        std::optional<std::string> get_smufl_font() const { return smufl_font; }
        void set_smufl_font(std::optional<std::string> value) { this->smufl_font = value; }

        std::optional<int64_t> get_staves() const { return staves; }
        void set_staves(std::optional<int64_t> value) { this->staves = value; }
    };

    class MultimeasureRest {
        public:
        MultimeasureRest() = default;
        virtual ~MultimeasureRest() = default;

        private:
        int64_t duration;
        std::optional<std::string> label;
        int64_t start;

        public:
        const int64_t & get_duration() const { return duration; }
        int64_t & get_mutable_duration() { return duration; }
        void set_duration(const int64_t & value) { this->duration = value; }

        std::optional<std::string> get_label() const { return label; }
        void set_label(std::optional<std::string> value) { this->label = value; }

        const int64_t & get_start() const { return start; }
        int64_t & get_mutable_start() { return start; }
        void set_start(const int64_t & value) { this->start = value; }
    };

    class LayoutChange {
        public:
        LayoutChange() = default;
        virtual ~LayoutChange() = default;

        private:
        std::string layout;
        EndClass location;

        public:
        const std::string & get_layout() const { return layout; }
        std::string & get_mutable_layout() { return layout; }
        void set_layout(const std::string & value) { this->layout = value; }

        const EndClass & get_location() const { return location; }
        EndClass & get_mutable_location() { return location; }
        void set_location(const EndClass & value) { this->location = value; }
    };

    class System {
        public:
        System() = default;
        virtual ~System() = default;

        private:
        std::optional<std::string> layout;
        std::optional<std::vector<LayoutChange>> layout_changes;
        int64_t measure;

        public:
        std::optional<std::string> get_layout() const { return layout; }
        void set_layout(std::optional<std::string> value) { this->layout = value; }

        std::optional<std::vector<LayoutChange>> get_layout_changes() const { return layout_changes; }
        void set_layout_changes(std::optional<std::vector<LayoutChange>> value) { this->layout_changes = value; }

        const int64_t & get_measure() const { return measure; }
        int64_t & get_mutable_measure() { return measure; }
        void set_measure(const int64_t & value) { this->measure = value; }
    };

    class Page {
        public:
        Page() = default;
        virtual ~Page() = default;

        private:
        std::optional<std::string> layout;
        std::vector<System> systems;

        public:
        std::optional<std::string> get_layout() const { return layout; }
        void set_layout(std::optional<std::string> value) { this->layout = value; }

        const std::vector<System> & get_systems() const { return systems; }
        std::vector<System> & get_mutable_systems() { return systems; }
        void set_systems(const std::vector<System> & value) { this->systems = value; }
    };

    class Score {
        public:
        Score() = default;
        virtual ~Score() = default;

        private:
        std::optional<std::string> layout;
        std::optional<std::vector<MultimeasureRest>> multimeasure_rests;
        std::string name;
        std::optional<std::vector<Page>> pages;

        public:
        std::optional<std::string> get_layout() const { return layout; }
        void set_layout(std::optional<std::string> value) { this->layout = value; }

        std::optional<std::vector<MultimeasureRest>> get_multimeasure_rests() const { return multimeasure_rests; }
        void set_multimeasure_rests(std::optional<std::vector<MultimeasureRest>> value) { this->multimeasure_rests = value; }

        const std::string & get_name() const { return name; }
        std::string & get_mutable_name() { return name; }
        void set_name(const std::string & value) { this->name = value; }

        std::optional<std::vector<Page>> get_pages() const { return pages; }
        void set_pages(std::optional<std::vector<Page>> value) { this->pages = value; }
    };

    /**
     * An encoding of Common Western Music Notation.
     */
    class MnxModel {
        public:
        MnxModel() = default;
        virtual ~MnxModel() = default;

        private:
        Global global;
        std::optional<std::vector<Layout>> layouts;
        Mnx mnx;
        std::vector<Part> parts;
        std::optional<std::vector<Score>> scores;

        public:
        const Global & get_global() const { return global; }
        Global & get_mutable_global() { return global; }
        void set_global(const Global & value) { this->global = value; }

        std::optional<std::vector<Layout>> get_layouts() const { return layouts; }
        void set_layouts(std::optional<std::vector<Layout>> value) { this->layouts = value; }

        const Mnx & get_mnx() const { return mnx; }
        Mnx & get_mutable_mnx() { return mnx; }
        void set_mnx(const Mnx & value) { this->mnx = value; }

        const std::vector<Part> & get_parts() const { return parts; }
        std::vector<Part> & get_mutable_parts() { return parts; }
        void set_parts(const std::vector<Part> & value) { this->parts = value; }

        std::optional<std::vector<Score>> get_scores() const { return scores; }
        void set_scores(std::optional<std::vector<Score>> value) { this->scores = value; }
    };
}

namespace mnx {
void from_json(const json & j, Barline & x);
void to_json(json & j, const Barline & x);

void from_json(const json & j, Ending & x);
void to_json(json & j, const Ending & x);

void from_json(const json & j, PositionClass & x);
void to_json(json & j, const PositionClass & x);

void from_json(const json & j, Fine & x);
void to_json(json & j, const Fine & x);

void from_json(const json & j, Jump & x);
void to_json(json & j, const Jump & x);

void from_json(const json & j, Key & x);
void to_json(json & j, const Key & x);

void from_json(const json & j, RepeatEnd & x);
void to_json(json & j, const RepeatEnd & x);

void from_json(const json & j, RepeatStart & x);
void to_json(json & j, const RepeatStart & x);

void from_json(const json & j, Segno & x);
void to_json(json & j, const Segno & x);

void from_json(const json & j, EndClass & x);
void to_json(json & j, const EndClass & x);

void from_json(const json & j, ValueClass & x);
void to_json(json & j, const ValueClass & x);

void from_json(const json & j, Tempo & x);
void to_json(json & j, const Tempo & x);

void from_json(const json & j, Time & x);
void to_json(json & j, const Time & x);

void from_json(const json & j, GlobalMeasure & x);
void to_json(json & j, const GlobalMeasure & x);

void from_json(const json & j, Style & x);
void to_json(json & j, const Style & x);

void from_json(const json & j, Global & x);
void to_json(json & j, const Global & x);

void from_json(const json & j, Source & x);
void to_json(json & j, const Source & x);

void from_json(const json & j, LayoutContent & x);
void to_json(json & j, const LayoutContent & x);

void from_json(const json & j, Layout & x);
void to_json(json & j, const Layout & x);

void from_json(const json & j, Support & x);
void to_json(json & j, const Support & x);

void from_json(const json & j, Mnx & x);
void to_json(json & j, const Mnx & x);

void from_json(const json & j, Hook & x);
void to_json(json & j, const Hook & x);

void from_json(const json & j, BeamElement & x);
void to_json(json & j, const BeamElement & x);

void from_json(const json & j, ClefClef & x);
void to_json(json & j, const ClefClef & x);

void from_json(const json & j, ClefElement & x);
void to_json(json & j, const ClefElement & x);

void from_json(const json & j, Lines & x);
void to_json(json & j, const Lines & x);

void from_json(const json & j, Lyrics & x);
void to_json(json & j, const Lyrics & x);

void from_json(const json & j, Accent & x);
void to_json(json & j, const Accent & x);

void from_json(const json & j, Breath & x);
void to_json(json & j, const Breath & x);

void from_json(const json & j, SoftAccent & x);
void to_json(json & j, const SoftAccent & x);

void from_json(const json & j, Spiccato & x);
void to_json(json & j, const Spiccato & x);

void from_json(const json & j, Staccatissimo & x);
void to_json(json & j, const Staccatissimo & x);

void from_json(const json & j, Staccato & x);
void to_json(json & j, const Staccato & x);

void from_json(const json & j, Stress & x);
void to_json(json & j, const Stress & x);

void from_json(const json & j, StrongAccent & x);
void to_json(json & j, const StrongAccent & x);

void from_json(const json & j, Tenuto & x);
void to_json(json & j, const Tenuto & x);

void from_json(const json & j, Tremolo & x);
void to_json(json & j, const Tremolo & x);

void from_json(const json & j, Unstress & x);
void to_json(json & j, const Unstress & x);

void from_json(const json & j, Markings & x);
void to_json(json & j, const Markings & x);

void from_json(const json & j, Enclosure & x);
void to_json(json & j, const Enclosure & x);

void from_json(const json & j, AccidentalDisplay & x);
void to_json(json & j, const AccidentalDisplay & x);

void from_json(const json & j, Perform & x);
void to_json(json & j, const Perform & x);

void from_json(const json & j, Pitch & x);
void to_json(json & j, const Pitch & x);

void from_json(const json & j, Tie & x);
void to_json(json & j, const Tie & x);

void from_json(const json & j, Note & x);
void to_json(json & j, const Note & x);

void from_json(const json & j, Rest & x);
void to_json(json & j, const Rest & x);

void from_json(const json & j, Slur & x);
void to_json(json & j, const Slur & x);

void from_json(const json & j, PurpleMnxSchema & x);
void to_json(json & j, const PurpleMnxSchema & x);

void from_json(const json & j, PurpleValue & x);
void to_json(json & j, const PurpleValue & x);

void from_json(const json & j, Inner & x);
void to_json(json & j, const Inner & x);

void from_json(const json & j, Content & x);
void to_json(json & j, const Content & x);

void from_json(const json & j, Sequence & x);
void to_json(json & j, const Sequence & x);

void from_json(const json & j, PartMeasure & x);
void to_json(json & j, const PartMeasure & x);

void from_json(const json & j, Part & x);
void to_json(json & j, const Part & x);

void from_json(const json & j, MultimeasureRest & x);
void to_json(json & j, const MultimeasureRest & x);

void from_json(const json & j, LayoutChange & x);
void to_json(json & j, const LayoutChange & x);

void from_json(const json & j, System & x);
void to_json(json & j, const System & x);

void from_json(const json & j, Page & x);
void to_json(json & j, const Page & x);

void from_json(const json & j, Score & x);
void to_json(json & j, const Score & x);

void from_json(const json & j, MnxModel & x);
void to_json(json & j, const MnxModel & x);

void from_json(const json & j, BarlineType & x);
void to_json(json & j, const BarlineType & x);

void from_json(const json & j, JumpType & x);
void to_json(json & j, const JumpType & x);

void from_json(const json & j, Base & x);
void to_json(json & j, const Base & x);

void from_json(const json & j, Stem & x);
void to_json(json & j, const Stem & x);

void from_json(const json & j, ContentSymbol & x);
void to_json(json & j, const ContentSymbol & x);

void from_json(const json & j, PurpleType & x);
void to_json(json & j, const PurpleType & x);

void from_json(const json & j, Direction & x);
void to_json(json & j, const Direction & x);

void from_json(const json & j, Sign & x);
void to_json(json & j, const Sign & x);

void from_json(const json & j, Bracket & x);
void to_json(json & j, const Bracket & x);

void from_json(const json & j, EnclosureSymbol & x);
void to_json(json & j, const EnclosureSymbol & x);

void from_json(const json & j, Step & x);
void to_json(json & j, const Step & x);

void from_json(const json & j, FluffyType & x);
void to_json(json & j, const FluffyType & x);

void from_json(const json & j, GraceType & x);
void to_json(json & j, const GraceType & x);

void from_json(const json & j, ShowNumber & x);
void to_json(json & j, const ShowNumber & x);

void from_json(const json & j, TentacledType & x);
void to_json(json & j, const TentacledType & x);
}
namespace nlohmann {
template <>
struct adl_serializer<std::variant<int64_t, std::string>> {
    static void from_json(const json & j, std::variant<int64_t, std::string> & x);
    static void to_json(json & j, const std::variant<int64_t, std::string> & x);
};
}
namespace mnx {
    inline void from_json(const json & j, Barline& x) {
        x.set_type(j.at("type").get<BarlineType>());
    }

    inline void to_json(json & j, const Barline & x) {
        j = json::object();
        j["type"] = x.get_type();
    }

    inline void from_json(const json & j, Ending& x) {
        x.set_ending_class(get_stack_optional<std::string>(j, "class"));
        x.set_color(get_stack_optional<std::string>(j, "color"));
        x.set_duration(j.at("duration").get<int64_t>());
        x.set_numbers(get_stack_optional<std::vector<int64_t>>(j, "numbers"));
        x.set_open(get_stack_optional<bool>(j, "open"));
    }

    inline void to_json(json & j, const Ending & x) {
        j = json::object();
        j["class"] = x.get_ending_class();
        j["color"] = x.get_color();
        j["duration"] = x.get_duration();
        j["numbers"] = x.get_numbers();
        j["open"] = x.get_open();
    }

    inline void from_json(const json & j, PositionClass& x) {
        x.set_fraction(j.at("fraction").get<std::vector<int64_t>>());
        x.set_grace_index(get_stack_optional<int64_t>(j, "graceIndex"));
    }

    inline void to_json(json & j, const PositionClass & x) {
        j = json::object();
        j["fraction"] = x.get_fraction();
        j["graceIndex"] = x.get_grace_index();
    }

    inline void from_json(const json & j, Fine& x) {
        x.set_fine_class(get_stack_optional<std::string>(j, "class"));
        x.set_color(get_stack_optional<std::string>(j, "color"));
        x.set_location(j.at("location").get<PositionClass>());
    }

    inline void to_json(json & j, const Fine & x) {
        j = json::object();
        j["class"] = x.get_fine_class();
        j["color"] = x.get_color();
        j["location"] = x.get_location();
    }

    inline void from_json(const json & j, Jump& x) {
        x.set_location(j.at("location").get<PositionClass>());
        x.set_type(j.at("type").get<JumpType>());
    }

    inline void to_json(json & j, const Jump & x) {
        j = json::object();
        j["location"] = x.get_location();
        j["type"] = x.get_type();
    }

    inline void from_json(const json & j, Key& x) {
        x.set_key_class(get_stack_optional<std::string>(j, "class"));
        x.set_color(get_stack_optional<std::string>(j, "color"));
        x.set_fifths(j.at("fifths").get<int64_t>());
    }

    inline void to_json(json & j, const Key & x) {
        j = json::object();
        j["class"] = x.get_key_class();
        j["color"] = x.get_color();
        j["fifths"] = x.get_fifths();
    }

    inline void from_json(const json & j, RepeatEnd& x) {
        x.set_times(get_stack_optional<int64_t>(j, "times"));
    }

    inline void to_json(json & j, const RepeatEnd & x) {
        j = json::object();
        j["times"] = x.get_times();
    }

    inline void from_json(const json & j, RepeatStart& x) {
    }

    inline void to_json(json & j, const RepeatStart & x) {
        j = json::object();
    }

    inline void from_json(const json & j, Segno& x) {
        x.set_segno_class(get_stack_optional<std::string>(j, "class"));
        x.set_color(get_stack_optional<std::string>(j, "color"));
        x.set_glyph(get_stack_optional<std::string>(j, "glyph"));
        x.set_location(j.at("location").get<PositionClass>());
    }

    inline void to_json(json & j, const Segno & x) {
        j = json::object();
        j["class"] = x.get_segno_class();
        j["color"] = x.get_color();
        j["glyph"] = x.get_glyph();
        j["location"] = x.get_location();
    }

    inline void from_json(const json & j, EndClass& x) {
        x.set_bar(j.at("bar").get<int64_t>());
        x.set_position(j.at("position").get<PositionClass>());
    }

    inline void to_json(json & j, const EndClass & x) {
        j = json::object();
        j["bar"] = x.get_bar();
        j["position"] = x.get_position();
    }

    inline void from_json(const json & j, ValueClass& x) {
        x.set_base(j.at("base").get<Base>());
        x.set_dots(get_stack_optional<int64_t>(j, "dots"));
    }

    inline void to_json(json & j, const ValueClass & x) {
        j = json::object();
        j["base"] = x.get_base();
        j["dots"] = x.get_dots();
    }

    inline void from_json(const json & j, Tempo& x) {
        x.set_bpm(j.at("bpm").get<int64_t>());
        x.set_location(get_stack_optional<EndClass>(j, "location"));
        x.set_value(j.at("value").get<ValueClass>());
    }

    inline void to_json(json & j, const Tempo & x) {
        j = json::object();
        j["bpm"] = x.get_bpm();
        j["location"] = x.get_location();
        j["value"] = x.get_value();
    }

    inline void from_json(const json & j, Time& x) {
        x.set_count(j.at("count").get<int64_t>());
        x.set_unit(j.at("unit").get<int64_t>());
    }

    inline void to_json(json & j, const Time & x) {
        j = json::object();
        j["count"] = x.get_count();
        j["unit"] = x.get_unit();
    }

    inline void from_json(const json & j, GlobalMeasure& x) {
        x.set_barline(get_stack_optional<Barline>(j, "barline"));
        x.set_ending(get_stack_optional<Ending>(j, "ending"));
        x.set_fine(get_stack_optional<Fine>(j, "fine"));
        x.set_index(get_stack_optional<int64_t>(j, "index"));
        x.set_jump(get_stack_optional<Jump>(j, "jump"));
        x.set_key(get_stack_optional<Key>(j, "key"));
        x.set_number(get_stack_optional<int64_t>(j, "number"));
        x.set_repeat_end(get_stack_optional<RepeatEnd>(j, "repeatEnd"));
        x.set_repeat_start(get_stack_optional<RepeatStart>(j, "repeatStart"));
        x.set_segno(get_stack_optional<Segno>(j, "segno"));
        x.set_tempos(get_stack_optional<std::vector<Tempo>>(j, "tempos"));
        x.set_time(get_stack_optional<Time>(j, "time"));
    }

    inline void to_json(json & j, const GlobalMeasure & x) {
        j = json::object();
        j["barline"] = x.get_barline();
        j["ending"] = x.get_ending();
        j["fine"] = x.get_fine();
        j["index"] = x.get_index();
        j["jump"] = x.get_jump();
        j["key"] = x.get_key();
        j["number"] = x.get_number();
        j["repeatEnd"] = x.get_repeat_end();
        j["repeatStart"] = x.get_repeat_start();
        j["segno"] = x.get_segno();
        j["tempos"] = x.get_tempos();
        j["time"] = x.get_time();
    }

    inline void from_json(const json & j, Style& x) {
        x.set_color(get_stack_optional<std::string>(j, "color"));
        x.set_selector(j.at("selector").get<std::string>());
    }

    inline void to_json(json & j, const Style & x) {
        j = json::object();
        j["color"] = x.get_color();
        j["selector"] = x.get_selector();
    }

    inline void from_json(const json & j, Global& x) {
        x.set_measures(j.at("measures").get<std::vector<GlobalMeasure>>());
        x.set_styles(get_stack_optional<std::vector<Style>>(j, "styles"));
    }

    inline void to_json(json & j, const Global & x) {
        j = json::object();
        j["measures"] = x.get_measures();
        j["styles"] = x.get_styles();
    }

    inline void from_json(const json & j, Source& x) {
        x.set_label(get_stack_optional<std::string>(j, "label"));
        x.set_labelref(get_stack_optional<std::string>(j, "labelref"));
        x.set_part(j.at("part").get<std::string>());
        x.set_staff(get_stack_optional<int64_t>(j, "staff"));
        x.set_stem(get_stack_optional<Stem>(j, "stem"));
        x.set_voice(get_stack_optional<std::string>(j, "voice"));
    }

    inline void to_json(json & j, const Source & x) {
        j = json::object();
        j["label"] = x.get_label();
        j["labelref"] = x.get_labelref();
        j["part"] = x.get_part();
        j["staff"] = x.get_staff();
        j["stem"] = x.get_stem();
        j["voice"] = x.get_voice();
    }

    inline void from_json(const json & j, LayoutContent& x) {
        x.set_content(get_stack_optional<std::vector<LayoutContent>>(j, "content"));
        x.set_label(get_stack_optional<std::string>(j, "label"));
        x.set_symbol(get_stack_optional<ContentSymbol>(j, "symbol"));
        x.set_type(j.at("type").get<PurpleType>());
        x.set_labelref(get_stack_optional<std::string>(j, "labelref"));
        x.set_sources(get_stack_optional<std::vector<Source>>(j, "sources"));
    }

    inline void to_json(json & j, const LayoutContent & x) {
        j = json::object();
        j["content"] = x.get_content();
        j["label"] = x.get_label();
        j["symbol"] = x.get_symbol();
        j["type"] = x.get_type();
        j["labelref"] = x.get_labelref();
        j["sources"] = x.get_sources();
    }

    inline void from_json(const json & j, Layout& x) {
        x.set_content(j.at("content").get<std::vector<LayoutContent>>());
        x.set_id(j.at("id").get<std::string>());
    }

    inline void to_json(json & j, const Layout & x) {
        j = json::object();
        j["content"] = x.get_content();
        j["id"] = x.get_id();
    }

    inline void from_json(const json & j, Support& x) {
        x.set_use_accidental_display(get_stack_optional<bool>(j, "useAccidentalDisplay"));
    }

    inline void to_json(json & j, const Support & x) {
        j = json::object();
        j["useAccidentalDisplay"] = x.get_use_accidental_display();
    }

    inline void from_json(const json & j, Mnx& x) {
        x.set_support(get_stack_optional<Support>(j, "support"));
        x.set_version(j.at("version").get<int64_t>());
    }

    inline void to_json(json & j, const Mnx & x) {
        j = json::object();
        j["support"] = x.get_support();
        j["version"] = x.get_version();
    }

    inline void from_json(const json & j, Hook& x) {
        x.set_direction(j.at("direction").get<Direction>());
        x.set_event(j.at("event").get<std::string>());
    }

    inline void to_json(json & j, const Hook & x) {
        j = json::object();
        j["direction"] = x.get_direction();
        j["event"] = x.get_event();
    }

    inline void from_json(const json & j, BeamElement& x) {
        x.set_events(j.at("events").get<std::vector<std::string>>());
        x.set_hooks(get_stack_optional<std::vector<Hook>>(j, "hooks"));
        x.set_inner(get_stack_optional<std::vector<BeamElement>>(j, "inner"));
    }

    inline void to_json(json & j, const BeamElement & x) {
        j = json::object();
        j["events"] = x.get_events();
        j["hooks"] = x.get_hooks();
        j["inner"] = x.get_inner();
    }

    inline void from_json(const json & j, ClefClef& x) {
        x.set_clef_class(get_stack_optional<std::string>(j, "class"));
        x.set_color(get_stack_optional<std::string>(j, "color"));
        x.set_glyph(get_stack_optional<std::string>(j, "glyph"));
        x.set_octave(get_stack_optional<int64_t>(j, "octave"));
        x.set_sign(j.at("sign").get<Sign>());
        x.set_staff_position(j.at("staffPosition").get<int64_t>());
    }

    inline void to_json(json & j, const ClefClef & x) {
        j = json::object();
        j["class"] = x.get_clef_class();
        j["color"] = x.get_color();
        j["glyph"] = x.get_glyph();
        j["octave"] = x.get_octave();
        j["sign"] = x.get_sign();
        j["staffPosition"] = x.get_staff_position();
    }

    inline void from_json(const json & j, ClefElement& x) {
        x.set_clef(j.at("clef").get<ClefClef>());
        x.set_position(get_stack_optional<PositionClass>(j, "position"));
        x.set_staff(get_stack_optional<int64_t>(j, "staff"));
    }

    inline void to_json(json & j, const ClefElement & x) {
        j = json::object();
        j["clef"] = x.get_clef();
        j["position"] = x.get_position();
        j["staff"] = x.get_staff();
    }

    inline void from_json(const json & j, Lines& x) {
    }

    inline void to_json(json & j, const Lines & x) {
        j = json::object();
    }

    inline void from_json(const json & j, Lyrics& x) {
        x.set_lines(get_stack_optional<Lines>(j, "lines"));
    }

    inline void to_json(json & j, const Lyrics & x) {
        j = json::object();
        j["lines"] = x.get_lines();
    }

    inline void from_json(const json & j, Accent& x) {
        x.set_pointing(get_stack_optional<Stem>(j, "pointing"));
    }

    inline void to_json(json & j, const Accent & x) {
        j = json::object();
        j["pointing"] = x.get_pointing();
    }

    inline void from_json(const json & j, Breath& x) {
        x.set_symbol(get_stack_optional<std::string>(j, "symbol"));
    }

    inline void to_json(json & j, const Breath & x) {
        j = json::object();
        j["symbol"] = x.get_symbol();
    }

    inline void from_json(const json & j, SoftAccent& x) {
    }

    inline void to_json(json & j, const SoftAccent & x) {
        j = json::object();
    }

    inline void from_json(const json & j, Spiccato& x) {
    }

    inline void to_json(json & j, const Spiccato & x) {
        j = json::object();
    }

    inline void from_json(const json & j, Staccatissimo& x) {
    }

    inline void to_json(json & j, const Staccatissimo & x) {
        j = json::object();
    }

    inline void from_json(const json & j, Staccato& x) {
    }

    inline void to_json(json & j, const Staccato & x) {
        j = json::object();
    }

    inline void from_json(const json & j, Stress& x) {
    }

    inline void to_json(json & j, const Stress & x) {
        j = json::object();
    }

    inline void from_json(const json & j, StrongAccent& x) {
        x.set_pointing(get_stack_optional<Stem>(j, "pointing"));
    }

    inline void to_json(json & j, const StrongAccent & x) {
        j = json::object();
        j["pointing"] = x.get_pointing();
    }

    inline void from_json(const json & j, Tenuto& x) {
    }

    inline void to_json(json & j, const Tenuto & x) {
        j = json::object();
    }

    inline void from_json(const json & j, Tremolo& x) {
        x.set_marks(j.at("marks").get<int64_t>());
    }

    inline void to_json(json & j, const Tremolo & x) {
        j = json::object();
        j["marks"] = x.get_marks();
    }

    inline void from_json(const json & j, Unstress& x) {
    }

    inline void to_json(json & j, const Unstress & x) {
        j = json::object();
    }

    inline void from_json(const json & j, Markings& x) {
        x.set_accent(get_stack_optional<Accent>(j, "accent"));
        x.set_breath(get_stack_optional<Breath>(j, "breath"));
        x.set_soft_accent(get_stack_optional<SoftAccent>(j, "softAccent"));
        x.set_spiccato(get_stack_optional<Spiccato>(j, "spiccato"));
        x.set_staccatissimo(get_stack_optional<Staccatissimo>(j, "staccatissimo"));
        x.set_staccato(get_stack_optional<Staccato>(j, "staccato"));
        x.set_stress(get_stack_optional<Stress>(j, "stress"));
        x.set_strong_accent(get_stack_optional<StrongAccent>(j, "strongAccent"));
        x.set_tenuto(get_stack_optional<Tenuto>(j, "tenuto"));
        x.set_tremolo(get_stack_optional<Tremolo>(j, "tremolo"));
        x.set_unstress(get_stack_optional<Unstress>(j, "unstress"));
    }

    inline void to_json(json & j, const Markings & x) {
        j = json::object();
        j["accent"] = x.get_accent();
        j["breath"] = x.get_breath();
        j["softAccent"] = x.get_soft_accent();
        j["spiccato"] = x.get_spiccato();
        j["staccatissimo"] = x.get_staccatissimo();
        j["staccato"] = x.get_staccato();
        j["stress"] = x.get_stress();
        j["strongAccent"] = x.get_strong_accent();
        j["tenuto"] = x.get_tenuto();
        j["tremolo"] = x.get_tremolo();
        j["unstress"] = x.get_unstress();
    }

    inline void from_json(const json & j, Enclosure& x) {
        x.set_symbol(j.at("symbol").get<EnclosureSymbol>());
    }

    inline void to_json(json & j, const Enclosure & x) {
        j = json::object();
        j["symbol"] = x.get_symbol();
    }

    inline void from_json(const json & j, AccidentalDisplay& x) {
        x.set_enclosure(get_stack_optional<Enclosure>(j, "enclosure"));
        x.set_show(j.at("show").get<bool>());
    }

    inline void to_json(json & j, const AccidentalDisplay & x) {
        j = json::object();
        j["enclosure"] = x.get_enclosure();
        j["show"] = x.get_show();
    }

    inline void from_json(const json & j, Perform& x) {
    }

    inline void to_json(json & j, const Perform & x) {
        j = json::object();
    }

    inline void from_json(const json & j, Pitch& x) {
        x.set_alter(get_stack_optional<int64_t>(j, "alter"));
        x.set_octave(j.at("octave").get<int64_t>());
        x.set_step(j.at("step").get<Step>());
    }

    inline void to_json(json & j, const Pitch & x) {
        j = json::object();
        j["alter"] = x.get_alter();
        j["octave"] = x.get_octave();
        j["step"] = x.get_step();
    }

    inline void from_json(const json & j, Tie& x) {
        x.set_location(get_stack_optional<std::string>(j, "location"));
        x.set_side(get_stack_optional<Stem>(j, "side"));
        x.set_target(get_stack_optional<std::string>(j, "target"));
    }

    inline void to_json(json & j, const Tie & x) {
        j = json::object();
        j["location"] = x.get_location();
        j["side"] = x.get_side();
        j["target"] = x.get_target();
    }

    inline void from_json(const json & j, Note& x) {
        x.set_accidental_display(get_stack_optional<AccidentalDisplay>(j, "accidentalDisplay"));
        x.set_note_class(get_stack_optional<std::string>(j, "class"));
        x.set_id(get_stack_optional<std::string>(j, "id"));
        x.set_perform(get_stack_optional<Perform>(j, "perform"));
        x.set_pitch(j.at("pitch").get<Pitch>());
        x.set_smufl_font(get_stack_optional<std::string>(j, "smuflFont"));
        x.set_staff(get_stack_optional<int64_t>(j, "staff"));
        x.set_tie(get_stack_optional<Tie>(j, "tie"));
    }

    inline void to_json(json & j, const Note & x) {
        j = json::object();
        j["accidentalDisplay"] = x.get_accidental_display();
        j["class"] = x.get_note_class();
        j["id"] = x.get_id();
        j["perform"] = x.get_perform();
        j["pitch"] = x.get_pitch();
        j["smuflFont"] = x.get_smufl_font();
        j["staff"] = x.get_staff();
        j["tie"] = x.get_tie();
    }

    inline void from_json(const json & j, Rest& x) {
        x.set_staff_position(get_stack_optional<int64_t>(j, "staffPosition"));
    }

    inline void to_json(json & j, const Rest & x) {
        j = json::object();
        j["staffPosition"] = x.get_staff_position();
    }

    inline void from_json(const json & j, Slur& x) {
        x.set_end_note(get_stack_optional<std::string>(j, "endNote"));
        x.set_line_type(get_stack_optional<std::string>(j, "lineType"));
        x.set_location(get_stack_optional<std::string>(j, "location"));
        x.set_side(get_stack_optional<Stem>(j, "side"));
        x.set_side_end(get_stack_optional<Stem>(j, "sideEnd"));
        x.set_start_note(get_stack_optional<std::string>(j, "startNote"));
        x.set_target(get_stack_optional<std::string>(j, "target"));
    }

    inline void to_json(json & j, const Slur & x) {
        j = json::object();
        j["endNote"] = x.get_end_note();
        j["lineType"] = x.get_line_type();
        j["location"] = x.get_location();
        j["side"] = x.get_side();
        j["sideEnd"] = x.get_side_end();
        j["startNote"] = x.get_start_note();
        j["target"] = x.get_target();
    }

    inline void from_json(const json & j, PurpleMnxSchema& x) {
        x.set_duration(get_stack_optional<ValueClass>(j, "duration"));
        x.set_id(get_stack_optional<std::string>(j, "id"));
        x.set_lyrics(get_stack_optional<Lyrics>(j, "lyrics"));
        x.set_markings(get_stack_optional<Markings>(j, "markings"));
        x.set_measure(get_stack_optional<bool>(j, "measure"));
        x.set_notes(get_stack_optional<std::vector<Note>>(j, "notes"));
        x.set_orient(get_stack_optional<std::string>(j, "orient"));
        x.set_rest(get_stack_optional<Rest>(j, "rest"));
        x.set_slurs(get_stack_optional<std::vector<Slur>>(j, "slurs"));
        x.set_smufl_font(get_stack_optional<std::string>(j, "smuflFont"));
        x.set_staff(get_stack_optional<int64_t>(j, "staff"));
        x.set_stem_direction(get_stack_optional<Stem>(j, "stemDirection"));
        x.set_type(j.at("type").get<FluffyType>());
    }

    inline void to_json(json & j, const PurpleMnxSchema & x) {
        j = json::object();
        j["duration"] = x.get_duration();
        j["id"] = x.get_id();
        j["lyrics"] = x.get_lyrics();
        j["markings"] = x.get_markings();
        j["measure"] = x.get_measure();
        j["notes"] = x.get_notes();
        j["orient"] = x.get_orient();
        j["rest"] = x.get_rest();
        j["slurs"] = x.get_slurs();
        j["smuflFont"] = x.get_smufl_font();
        j["staff"] = x.get_staff();
        j["stemDirection"] = x.get_stem_direction();
        j["type"] = x.get_type();
    }

    inline void from_json(const json & j, PurpleValue& x) {
        x.set_base(get_stack_optional<Base>(j, "base"));
        x.set_dots(get_stack_optional<int64_t>(j, "dots"));
        x.set_duration(get_stack_optional<ValueClass>(j, "duration"));
        x.set_multiple(get_stack_optional<int64_t>(j, "multiple"));
    }

    inline void to_json(json & j, const PurpleValue & x) {
        j = json::object();
        j["base"] = x.get_base();
        j["dots"] = x.get_dots();
        j["duration"] = x.get_duration();
        j["multiple"] = x.get_multiple();
    }

    inline void from_json(const json & j, Inner& x) {
        x.set_duration(j.at("duration").get<ValueClass>());
        x.set_multiple(j.at("multiple").get<int64_t>());
    }

    inline void to_json(json & j, const Inner & x) {
        j = json::object();
        j["duration"] = x.get_duration();
        j["multiple"] = x.get_multiple();
    }

    inline void from_json(const json & j, Content& x) {
        x.set_duration(get_stack_optional<PurpleValue>(j, "duration"));
        x.set_id(get_stack_optional<std::string>(j, "id"));
        x.set_lyrics(get_stack_optional<Lyrics>(j, "lyrics"));
        x.set_markings(get_stack_optional<Markings>(j, "markings"));
        x.set_measure(get_stack_optional<bool>(j, "measure"));
        x.set_notes(get_stack_optional<std::vector<Note>>(j, "notes"));
        x.set_orient(get_stack_optional<std::string>(j, "orient"));
        x.set_rest(get_stack_optional<Rest>(j, "rest"));
        x.set_slurs(get_stack_optional<std::vector<Slur>>(j, "slurs"));
        x.set_smufl_font(get_stack_optional<std::string>(j, "smuflFont"));
        x.set_staff(get_stack_optional<int64_t>(j, "staff"));
        x.set_stem_direction(get_stack_optional<Stem>(j, "stemDirection"));
        x.set_type(j.at("type").get<TentacledType>());
        x.set_content_class(get_stack_optional<std::string>(j, "class"));
        x.set_color(get_stack_optional<std::string>(j, "color"));
        x.set_content(get_stack_optional<std::vector<PurpleMnxSchema>>(j, "content"));
        x.set_grace_type(get_stack_optional<GraceType>(j, "graceType"));
        x.set_slash(get_stack_optional<bool>(j, "slash"));
        x.set_bracket(get_stack_optional<Bracket>(j, "bracket"));
        x.set_inner(get_stack_optional<Inner>(j, "inner"));
        x.set_outer(get_stack_optional<Inner>(j, "outer"));
        x.set_show_number(get_stack_optional<ShowNumber>(j, "showNumber"));
        x.set_show_value(get_stack_optional<ShowNumber>(j, "showValue"));
        x.set_end(get_stack_optional<EndClass>(j, "end"));
        x.set_value(get_stack_optional<std::variant<int64_t, std::string>>(j, "value"));
        x.set_glyph(get_stack_optional<std::string>(j, "glyph"));
    }

    inline void to_json(json & j, const Content & x) {
        j = json::object();
        j["duration"] = x.get_duration();
        j["id"] = x.get_id();
        j["lyrics"] = x.get_lyrics();
        j["markings"] = x.get_markings();
        j["measure"] = x.get_measure();
        j["notes"] = x.get_notes();
        j["orient"] = x.get_orient();
        j["rest"] = x.get_rest();
        j["slurs"] = x.get_slurs();
        j["smuflFont"] = x.get_smufl_font();
        j["staff"] = x.get_staff();
        j["stemDirection"] = x.get_stem_direction();
        j["type"] = x.get_type();
        j["class"] = x.get_content_class();
        j["color"] = x.get_color();
        j["content"] = x.get_content();
        j["graceType"] = x.get_grace_type();
        j["slash"] = x.get_slash();
        j["bracket"] = x.get_bracket();
        j["inner"] = x.get_inner();
        j["outer"] = x.get_outer();
        j["showNumber"] = x.get_show_number();
        j["showValue"] = x.get_show_value();
        j["end"] = x.get_end();
        j["value"] = x.get_value();
        j["glyph"] = x.get_glyph();
    }

    inline void from_json(const json & j, Sequence& x) {
        x.set_content(j.at("content").get<std::vector<Content>>());
        x.set_orient(get_stack_optional<std::string>(j, "orient"));
        x.set_staff(get_stack_optional<int64_t>(j, "staff"));
        x.set_voice(get_stack_optional<std::string>(j, "voice"));
    }

    inline void to_json(json & j, const Sequence & x) {
        j = json::object();
        j["content"] = x.get_content();
        j["orient"] = x.get_orient();
        j["staff"] = x.get_staff();
        j["voice"] = x.get_voice();
    }

    inline void from_json(const json & j, PartMeasure& x) {
        x.set_beams(get_stack_optional<std::vector<BeamElement>>(j, "beams"));
        x.set_clefs(get_stack_optional<std::vector<ClefElement>>(j, "clefs"));
        x.set_sequences(j.at("sequences").get<std::vector<Sequence>>());
    }

    inline void to_json(json & j, const PartMeasure & x) {
        j = json::object();
        j["beams"] = x.get_beams();
        j["clefs"] = x.get_clefs();
        j["sequences"] = x.get_sequences();
    }

    inline void from_json(const json & j, Part& x) {
        x.set_id(get_stack_optional<std::string>(j, "id"));
        x.set_measures(get_stack_optional<std::vector<PartMeasure>>(j, "measures"));
        x.set_name(get_stack_optional<std::string>(j, "name"));
        x.set_short_name(get_stack_optional<std::string>(j, "shortName"));
        x.set_smufl_font(get_stack_optional<std::string>(j, "smuflFont"));
        x.set_staves(get_stack_optional<int64_t>(j, "staves"));
    }

    inline void to_json(json & j, const Part & x) {
        j = json::object();
        j["id"] = x.get_id();
        j["measures"] = x.get_measures();
        j["name"] = x.get_name();
        j["shortName"] = x.get_short_name();
        j["smuflFont"] = x.get_smufl_font();
        j["staves"] = x.get_staves();
    }

    inline void from_json(const json & j, MultimeasureRest& x) {
        x.set_duration(j.at("duration").get<int64_t>());
        x.set_label(get_stack_optional<std::string>(j, "label"));
        x.set_start(j.at("start").get<int64_t>());
    }

    inline void to_json(json & j, const MultimeasureRest & x) {
        j = json::object();
        j["duration"] = x.get_duration();
        j["label"] = x.get_label();
        j["start"] = x.get_start();
    }

    inline void from_json(const json & j, LayoutChange& x) {
        x.set_layout(j.at("layout").get<std::string>());
        x.set_location(j.at("location").get<EndClass>());
    }

    inline void to_json(json & j, const LayoutChange & x) {
        j = json::object();
        j["layout"] = x.get_layout();
        j["location"] = x.get_location();
    }

    inline void from_json(const json & j, System& x) {
        x.set_layout(get_stack_optional<std::string>(j, "layout"));
        x.set_layout_changes(get_stack_optional<std::vector<LayoutChange>>(j, "layoutChanges"));
        x.set_measure(j.at("measure").get<int64_t>());
    }

    inline void to_json(json & j, const System & x) {
        j = json::object();
        j["layout"] = x.get_layout();
        j["layoutChanges"] = x.get_layout_changes();
        j["measure"] = x.get_measure();
    }

    inline void from_json(const json & j, Page& x) {
        x.set_layout(get_stack_optional<std::string>(j, "layout"));
        x.set_systems(j.at("systems").get<std::vector<System>>());
    }

    inline void to_json(json & j, const Page & x) {
        j = json::object();
        j["layout"] = x.get_layout();
        j["systems"] = x.get_systems();
    }

    inline void from_json(const json & j, Score& x) {
        x.set_layout(get_stack_optional<std::string>(j, "layout"));
        x.set_multimeasure_rests(get_stack_optional<std::vector<MultimeasureRest>>(j, "multimeasureRests"));
        x.set_name(j.at("name").get<std::string>());
        x.set_pages(get_stack_optional<std::vector<Page>>(j, "pages"));
    }

    inline void to_json(json & j, const Score & x) {
        j = json::object();
        j["layout"] = x.get_layout();
        j["multimeasureRests"] = x.get_multimeasure_rests();
        j["name"] = x.get_name();
        j["pages"] = x.get_pages();
    }

    inline void from_json(const json & j, MnxModel& x) {
        x.set_global(j.at("global").get<Global>());
        x.set_layouts(get_stack_optional<std::vector<Layout>>(j, "layouts"));
        x.set_mnx(j.at("mnx").get<Mnx>());
        x.set_parts(j.at("parts").get<std::vector<Part>>());
        x.set_scores(get_stack_optional<std::vector<Score>>(j, "scores"));
    }

    inline void to_json(json & j, const MnxModel & x) {
        j = json::object();
        j["global"] = x.get_global();
        j["layouts"] = x.get_layouts();
        j["mnx"] = x.get_mnx();
        j["parts"] = x.get_parts();
        j["scores"] = x.get_scores();
    }

    inline void from_json(const json & j, BarlineType & x) {
        if (j == "dashed") x = BarlineType::DASHED;
        else if (j == "dotted") x = BarlineType::DOTTED;
        else if (j == "double") x = BarlineType::DOUBLE;
        else if (j == "final") x = BarlineType::FINAL;
        else if (j == "heavy") x = BarlineType::HEAVY;
        else if (j == "heavyHeavy") x = BarlineType::HEAVY_HEAVY;
        else if (j == "heavyLight") x = BarlineType::HEAVY_LIGHT;
        else if (j == "noBarline") x = BarlineType::NO_BARLINE;
        else if (j == "regular") x = BarlineType::REGULAR;
        else if (j == "short") x = BarlineType::SHORT;
        else if (j == "tick") x = BarlineType::TICK;
        else { throw std::runtime_error("Input JSON does not conform to schema!"); }
    }

    inline void to_json(json & j, const BarlineType & x) {
        switch (x) {
            case BarlineType::DASHED: j = "dashed"; break;
            case BarlineType::DOTTED: j = "dotted"; break;
            case BarlineType::DOUBLE: j = "double"; break;
            case BarlineType::FINAL: j = "final"; break;
            case BarlineType::HEAVY: j = "heavy"; break;
            case BarlineType::HEAVY_HEAVY: j = "heavyHeavy"; break;
            case BarlineType::HEAVY_LIGHT: j = "heavyLight"; break;
            case BarlineType::NO_BARLINE: j = "noBarline"; break;
            case BarlineType::REGULAR: j = "regular"; break;
            case BarlineType::SHORT: j = "short"; break;
            case BarlineType::TICK: j = "tick"; break;
            default: throw std::runtime_error("Unexpected value in enumeration \"[object Object]\": " + std::to_string(static_cast<int>(x)));
        }
    }

    inline void from_json(const json & j, JumpType & x) {
        if (j == "dsalfine") x = JumpType::DSALFINE;
        else if (j == "segno") x = JumpType::SEGNO;
        else { throw std::runtime_error("Input JSON does not conform to schema!"); }
    }

    inline void to_json(json & j, const JumpType & x) {
        switch (x) {
            case JumpType::DSALFINE: j = "dsalfine"; break;
            case JumpType::SEGNO: j = "segno"; break;
            default: throw std::runtime_error("Unexpected value in enumeration \"[object Object]\": " + std::to_string(static_cast<int>(x)));
        }
    }

    inline void from_json(const json & j, Base & x) {
        static std::unordered_map<std::string, Base> enumValues {
            {"breve", Base::BREVE},
            {"duplexMaxima", Base::DUPLEX_MAXIMA},
            {"eighth", Base::EIGHTH},
            {"half", Base::HALF},
            {"longa", Base::LONGA},
            {"maxima", Base::MAXIMA},
            {"quarter", Base::QUARTER},
            {"1024th", Base::THE_1024_TH},
            {"128th", Base::THE_128_TH},
            {"16th", Base::THE_16_TH},
            {"2048th", Base::THE_2048_TH},
            {"256th", Base::THE_256_TH},
            {"32nd", Base::THE_32_ND},
            {"4096th", Base::THE_4096_TH},
            {"512th", Base::THE_512_TH},
            {"64th", Base::THE_64_TH},
            {"whole", Base::WHOLE},
        };
        auto iter = enumValues.find(j.get<std::string>());
        if (iter != enumValues.end()) {
            x = iter->second;
        }
    }

    inline void to_json(json & j, const Base & x) {
        switch (x) {
            case Base::BREVE: j = "breve"; break;
            case Base::DUPLEX_MAXIMA: j = "duplexMaxima"; break;
            case Base::EIGHTH: j = "eighth"; break;
            case Base::HALF: j = "half"; break;
            case Base::LONGA: j = "longa"; break;
            case Base::MAXIMA: j = "maxima"; break;
            case Base::QUARTER: j = "quarter"; break;
            case Base::THE_1024_TH: j = "1024th"; break;
            case Base::THE_128_TH: j = "128th"; break;
            case Base::THE_16_TH: j = "16th"; break;
            case Base::THE_2048_TH: j = "2048th"; break;
            case Base::THE_256_TH: j = "256th"; break;
            case Base::THE_32_ND: j = "32nd"; break;
            case Base::THE_4096_TH: j = "4096th"; break;
            case Base::THE_512_TH: j = "512th"; break;
            case Base::THE_64_TH: j = "64th"; break;
            case Base::WHOLE: j = "whole"; break;
            default: throw std::runtime_error("Unexpected value in enumeration \"[object Object]\": " + std::to_string(static_cast<int>(x)));
        }
    }

    inline void from_json(const json & j, Stem & x) {
        if (j == "down") x = Stem::DOWN;
        else if (j == "up") x = Stem::UP;
        else { throw std::runtime_error("Input JSON does not conform to schema!"); }
    }

    inline void to_json(json & j, const Stem & x) {
        switch (x) {
            case Stem::DOWN: j = "down"; break;
            case Stem::UP: j = "up"; break;
            default: throw std::runtime_error("Unexpected value in enumeration \"[object Object]\": " + std::to_string(static_cast<int>(x)));
        }
    }

    inline void from_json(const json & j, ContentSymbol & x) {
        if (j == "brace") x = ContentSymbol::BRACE;
        else if (j == "bracket") x = ContentSymbol::BRACKET;
        else if (j == "noSymbol") x = ContentSymbol::NO_SYMBOL;
        else { throw std::runtime_error("Input JSON does not conform to schema!"); }
    }

    inline void to_json(json & j, const ContentSymbol & x) {
        switch (x) {
            case ContentSymbol::BRACE: j = "brace"; break;
            case ContentSymbol::BRACKET: j = "bracket"; break;
            case ContentSymbol::NO_SYMBOL: j = "noSymbol"; break;
            default: throw std::runtime_error("Unexpected value in enumeration \"[object Object]\": " + std::to_string(static_cast<int>(x)));
        }
    }

    inline void from_json(const json & j, PurpleType & x) {
        if (j == "group") x = PurpleType::GROUP;
        else if (j == "staff") x = PurpleType::STAFF;
        else { throw std::runtime_error("Input JSON does not conform to schema!"); }
    }

    inline void to_json(json & j, const PurpleType & x) {
        switch (x) {
            case PurpleType::GROUP: j = "group"; break;
            case PurpleType::STAFF: j = "staff"; break;
            default: throw std::runtime_error("Unexpected value in enumeration \"[object Object]\": " + std::to_string(static_cast<int>(x)));
        }
    }

    inline void from_json(const json & j, Direction & x) {
        if (j == "left") x = Direction::LEFT;
        else if (j == "right") x = Direction::RIGHT;
        else { throw std::runtime_error("Input JSON does not conform to schema!"); }
    }

    inline void to_json(json & j, const Direction & x) {
        switch (x) {
            case Direction::LEFT: j = "left"; break;
            case Direction::RIGHT: j = "right"; break;
            default: throw std::runtime_error("Unexpected value in enumeration \"[object Object]\": " + std::to_string(static_cast<int>(x)));
        }
    }

    inline void from_json(const json & j, Sign & x) {
        if (j == "C") x = Sign::C;
        else if (j == "F") x = Sign::F;
        else if (j == "G") x = Sign::G;
        else { throw std::runtime_error("Input JSON does not conform to schema!"); }
    }

    inline void to_json(json & j, const Sign & x) {
        switch (x) {
            case Sign::C: j = "C"; break;
            case Sign::F: j = "F"; break;
            case Sign::G: j = "G"; break;
            default: throw std::runtime_error("Unexpected value in enumeration \"[object Object]\": " + std::to_string(static_cast<int>(x)));
        }
    }

    inline void from_json(const json & j, Bracket & x) {
        if (j == "auto") x = Bracket::AUTO;
        else if (j == "no") x = Bracket::NO;
        else if (j == "yes") x = Bracket::YES;
        else { throw std::runtime_error("Input JSON does not conform to schema!"); }
    }

    inline void to_json(json & j, const Bracket & x) {
        switch (x) {
            case Bracket::AUTO: j = "auto"; break;
            case Bracket::NO: j = "no"; break;
            case Bracket::YES: j = "yes"; break;
            default: throw std::runtime_error("Unexpected value in enumeration \"[object Object]\": " + std::to_string(static_cast<int>(x)));
        }
    }

    inline void from_json(const json & j, EnclosureSymbol & x) {
        if (j == "brackets") x = EnclosureSymbol::BRACKETS;
        else if (j == "parentheses") x = EnclosureSymbol::PARENTHESES;
        else { throw std::runtime_error("Input JSON does not conform to schema!"); }
    }

    inline void to_json(json & j, const EnclosureSymbol & x) {
        switch (x) {
            case EnclosureSymbol::BRACKETS: j = "brackets"; break;
            case EnclosureSymbol::PARENTHESES: j = "parentheses"; break;
            default: throw std::runtime_error("Unexpected value in enumeration \"[object Object]\": " + std::to_string(static_cast<int>(x)));
        }
    }

    inline void from_json(const json & j, Step & x) {
        if (j == "A") x = Step::A;
        else if (j == "B") x = Step::B;
        else if (j == "C") x = Step::C;
        else if (j == "D") x = Step::D;
        else if (j == "E") x = Step::E;
        else if (j == "F") x = Step::F;
        else if (j == "G") x = Step::G;
        else { throw std::runtime_error("Input JSON does not conform to schema!"); }
    }

    inline void to_json(json & j, const Step & x) {
        switch (x) {
            case Step::A: j = "A"; break;
            case Step::B: j = "B"; break;
            case Step::C: j = "C"; break;
            case Step::D: j = "D"; break;
            case Step::E: j = "E"; break;
            case Step::F: j = "F"; break;
            case Step::G: j = "G"; break;
            default: throw std::runtime_error("Unexpected value in enumeration \"[object Object]\": " + std::to_string(static_cast<int>(x)));
        }
    }

    inline void from_json(const json & j, FluffyType & x) {
        if (j == "event") x = FluffyType::EVENT;
        else { throw std::runtime_error("Input JSON does not conform to schema!"); }
    }

    inline void to_json(json & j, const FluffyType & x) {
        switch (x) {
            case FluffyType::EVENT: j = "event"; break;
            default: throw std::runtime_error("Unexpected value in enumeration \"[object Object]\": " + std::to_string(static_cast<int>(x)));
        }
    }

    inline void from_json(const json & j, GraceType & x) {
        if (j == "makeTime") x = GraceType::MAKE_TIME;
        else if (j == "stealFollowing") x = GraceType::STEAL_FOLLOWING;
        else if (j == "stealPrevious") x = GraceType::STEAL_PREVIOUS;
        else { throw std::runtime_error("Input JSON does not conform to schema!"); }
    }

    inline void to_json(json & j, const GraceType & x) {
        switch (x) {
            case GraceType::MAKE_TIME: j = "makeTime"; break;
            case GraceType::STEAL_FOLLOWING: j = "stealFollowing"; break;
            case GraceType::STEAL_PREVIOUS: j = "stealPrevious"; break;
            default: throw std::runtime_error("Unexpected value in enumeration \"[object Object]\": " + std::to_string(static_cast<int>(x)));
        }
    }

    inline void from_json(const json & j, ShowNumber & x) {
        if (j == "both") x = ShowNumber::BOTH;
        else if (j == "inner") x = ShowNumber::INNER;
        else if (j == "noNumber") x = ShowNumber::NO_NUMBER;
        else { throw std::runtime_error("Input JSON does not conform to schema!"); }
    }

    inline void to_json(json & j, const ShowNumber & x) {
        switch (x) {
            case ShowNumber::BOTH: j = "both"; break;
            case ShowNumber::INNER: j = "inner"; break;
            case ShowNumber::NO_NUMBER: j = "noNumber"; break;
            default: throw std::runtime_error("Unexpected value in enumeration \"[object Object]\": " + std::to_string(static_cast<int>(x)));
        }
    }

    inline void from_json(const json & j, TentacledType & x) {
        if (j == "dynamic") x = TentacledType::DYNAMIC;
        else if (j == "event") x = TentacledType::EVENT;
        else if (j == "grace") x = TentacledType::GRACE;
        else if (j == "ottava") x = TentacledType::OTTAVA;
        else if (j == "space") x = TentacledType::SPACE;
        else if (j == "tuplet") x = TentacledType::TUPLET;
        else { throw std::runtime_error("Input JSON does not conform to schema!"); }
    }

    inline void to_json(json & j, const TentacledType & x) {
        switch (x) {
            case TentacledType::DYNAMIC: j = "dynamic"; break;
            case TentacledType::EVENT: j = "event"; break;
            case TentacledType::GRACE: j = "grace"; break;
            case TentacledType::OTTAVA: j = "ottava"; break;
            case TentacledType::SPACE: j = "space"; break;
            case TentacledType::TUPLET: j = "tuplet"; break;
            default: throw std::runtime_error("Unexpected value in enumeration \"[object Object]\": " + std::to_string(static_cast<int>(x)));
        }
    }
}
namespace nlohmann {
    inline void adl_serializer<std::variant<int64_t, std::string>>::from_json(const json & j, std::variant<int64_t, std::string> & x) {
        if (j.is_number_integer())
            x = j.get<int64_t>();
        else if (j.is_string())
            x = j.get<std::string>();
        else throw std::runtime_error("Could not deserialise!");
    }

    inline void adl_serializer<std::variant<int64_t, std::string>>::to_json(json & j, const std::variant<int64_t, std::string> & x) {
        switch (x.index()) {
            case 0:
                j = std::get<int64_t>(x);
                break;
            case 1:
                j = std::get<std::string>(x);
                break;
            default: throw std::runtime_error("Input JSON does not conform to schema!");
        }
    }
}
