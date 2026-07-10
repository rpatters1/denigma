#include "musicxml.h"

#include "denigma/classify/articulations.h"
#include "denigma/classify/barlines.h"
#include "denigma/classify/dynamics.h"
#include "denigma/classify/noteheads.h"
#include "formats/enum_conversion_macros.h"

#include "mx/api/BarlineData.h"
#include "mx/api/ClefData.h"
#include "mx/api/CurveData.h"
#include "mx/api/DurationData.h"
#include "mx/api/MarkData.h"
#include "mx/api/NoteData.h"
#include "mx/api/OttavaData.h"
#include "mx/api/PartGroupData.h"
#include "mx/api/PitchData.h"
#include "mx/api/PositionData.h"
#include "mx/api/RehearsalData.h"

using namespace musx::dom;
using BarlineType = musx::dom::others::Measure::BarlineType;

namespace denigma {
namespace formats {
namespace musicxml {
namespace detail {

DEFINE_ENUM_CONVERT_TEMPLATE

BEGIN_ENUM_CONVERSION(AlignJustify, mx::api::HorizontalAlignment)
    case AlignJustify::Left: return mx::api::HorizontalAlignment::left;
    case AlignJustify::Right: return mx::api::HorizontalAlignment::right;
    case AlignJustify::Center: return mx::api::HorizontalAlignment::center;
END_ENUM_CONVERSION

BEGIN_ENUM_CONVERSION(classify::articulation::ArticulationMark::Type, mx::api::MarkType)
    case classify::articulation::ArticulationMark::Type::Accent: return mx::api::MarkType::accent;
    case classify::articulation::ArticulationMark::Type::BrassDoit: return mx::api::MarkType::doit;
    case classify::articulation::ArticulationMark::Type::BrassFalloff: return mx::api::MarkType::falloff;
    case classify::articulation::ArticulationMark::Type::BrassPlop: return mx::api::MarkType::plop;
    case classify::articulation::ArticulationMark::Type::BrassScoop: return mx::api::MarkType::scoop;
    case classify::articulation::ArticulationMark::Type::SoftAccent: return mx::api::MarkType::softAccent;
    case classify::articulation::ArticulationMark::Type::Spiccato: return mx::api::MarkType::spiccato;
    case classify::articulation::ArticulationMark::Type::Staccatissimo: return mx::api::MarkType::staccatissimo;
    case classify::articulation::ArticulationMark::Type::Staccato: return mx::api::MarkType::staccato;
    case classify::articulation::ArticulationMark::Type::Stress: return mx::api::MarkType::stress;
    case classify::articulation::ArticulationMark::Type::StrongAccent: return mx::api::MarkType::strongAccent;
    case classify::articulation::ArticulationMark::Type::Tenuto: return mx::api::MarkType::tenuto;
    case classify::articulation::ArticulationMark::Type::Unstress: return mx::api::MarkType::unstress;
END_ENUM_CONVERSION

BEGIN_ENUM_CONVERSION(BarlineType, mx::api::BarlineType)
    case BarlineType::None: return mx::api::BarlineType::none;
    case BarlineType::OptionsDefault: return mx::api::BarlineType::unspecified;
    case BarlineType::Normal: return mx::api::BarlineType::normal;
    case BarlineType::Double: return mx::api::BarlineType::lightLight;
    case BarlineType::Final: return mx::api::BarlineType::lightHeavy;
    case BarlineType::Solid: return mx::api::BarlineType::heavy;
    case BarlineType::Dashed: return mx::api::BarlineType::dashed;
    case BarlineType::Tick: return mx::api::BarlineType::tick;
    case BarlineType::Custom: return mx::api::BarlineType::unsupported;
END_ENUM_CONVERSION

BEGIN_ENUM_CONVERSION(classify::barline::BarlineType, mx::api::BarlineType)
    case classify::barline::BarlineType::Unsupported: return mx::api::BarlineType::unsupported;
    case classify::barline::BarlineType::NoBarline: return mx::api::BarlineType::none;
    case classify::barline::BarlineType::Regular: return mx::api::BarlineType::normal;
    case classify::barline::BarlineType::Double: return mx::api::BarlineType::lightLight;
    case classify::barline::BarlineType::Final: return mx::api::BarlineType::lightHeavy;
    case classify::barline::BarlineType::Heavy: return mx::api::BarlineType::heavy;
    case classify::barline::BarlineType::Dashed: return mx::api::BarlineType::dashed;
    case classify::barline::BarlineType::Tick: return mx::api::BarlineType::tick;
END_ENUM_CONVERSION

BEGIN_ENUM_CONVERSION(details::Bracket::BracketStyle, mx::api::BracketType)
    case details::Bracket::BracketStyle::None: return mx::api::BracketType::none;
    case details::Bracket::BracketStyle::ThickLine: return mx::api::BracketType::line;
    case details::Bracket::BracketStyle::BracketStraightHooks: return mx::api::BracketType::bracket;
    case details::Bracket::BracketStyle::PianoBrace: return mx::api::BracketType::brace;
    case details::Bracket::BracketStyle::Unknown4: return mx::api::BracketType::unspecified;
    case details::Bracket::BracketStyle::Unknown5: return mx::api::BracketType::unspecified;
    case details::Bracket::BracketStyle::BracketCurvedHooks: return mx::api::BracketType::bracket;
    case details::Bracket::BracketStyle::Unknown7: return mx::api::BracketType::unspecified;
    case details::Bracket::BracketStyle::DeskBracket: return mx::api::BracketType::square;
END_ENUM_CONVERSION

BEGIN_ENUM_CONVERSION(music_theory::ClefType, mx::api::ClefSymbol)
    case music_theory::ClefType::Unknown: return mx::api::ClefSymbol::g;
    case music_theory::ClefType::G: return mx::api::ClefSymbol::g;
    case music_theory::ClefType::F: return mx::api::ClefSymbol::f;
    case music_theory::ClefType::C: return mx::api::ClefSymbol::c;
    case music_theory::ClefType::Percussion1: return mx::api::ClefSymbol::percussion;
    case music_theory::ClefType::Percussion2: return mx::api::ClefSymbol::percussion;
    case music_theory::ClefType::Tab: return mx::api::ClefSymbol::tab;
    case music_theory::ClefType::TabSerif: return mx::api::ClefSymbol::tab;
END_ENUM_CONVERSION

BEGIN_ENUM_CONVERSION(CurveContourDirection, mx::api::CurveOrientation)
    case CurveContourDirection::Unspecified: return mx::api::CurveOrientation::unspecified;
    case CurveContourDirection::Up: return mx::api::CurveOrientation::overhand;
    case CurveContourDirection::Down: return mx::api::CurveOrientation::underhand;
END_ENUM_CONVERSION

BEGIN_ENUM_CONVERSION(classify::dynamics::Dynamic, mx::api::MarkType)
    case classify::dynamics::Dynamic::None: return mx::api::MarkType::unspecified;
    case classify::dynamics::Dynamic::Other: return mx::api::MarkType::otherDynamics;
    case classify::dynamics::Dynamic::pppppp: return mx::api::MarkType::pppppp;
    case classify::dynamics::Dynamic::ppppp: return mx::api::MarkType::ppppp;
    case classify::dynamics::Dynamic::pppp: return mx::api::MarkType::pppp;
    case classify::dynamics::Dynamic::ppp: return mx::api::MarkType::ppp;
    case classify::dynamics::Dynamic::pp: return mx::api::MarkType::pp;
    case classify::dynamics::Dynamic::p: return mx::api::MarkType::p;
    case classify::dynamics::Dynamic::mp: return mx::api::MarkType::mp;
    case classify::dynamics::Dynamic::mf: return mx::api::MarkType::mf;
    case classify::dynamics::Dynamic::f: return mx::api::MarkType::f;
    case classify::dynamics::Dynamic::ff: return mx::api::MarkType::ff;
    case classify::dynamics::Dynamic::fff: return mx::api::MarkType::fff;
    case classify::dynamics::Dynamic::ffff: return mx::api::MarkType::ffff;
    case classify::dynamics::Dynamic::fffff: return mx::api::MarkType::fffff;
    case classify::dynamics::Dynamic::ffffff: return mx::api::MarkType::ffffff;
    case classify::dynamics::Dynamic::fp: return mx::api::MarkType::fp;
    case classify::dynamics::Dynamic::ffp: return mx::api::MarkType::otherDynamics;
    case classify::dynamics::Dynamic::fz: return mx::api::MarkType::fz;
    case classify::dynamics::Dynamic::ffz: return mx::api::MarkType::otherDynamics;
    case classify::dynamics::Dynamic::pf: return mx::api::MarkType::pf;
    case classify::dynamics::Dynamic::sf: return mx::api::MarkType::sf;
    case classify::dynamics::Dynamic::sfp: return mx::api::MarkType::sfp;
    case classify::dynamics::Dynamic::sfpp: return mx::api::MarkType::sfpp;
    case classify::dynamics::Dynamic::sfz: return mx::api::MarkType::sfz;
    case classify::dynamics::Dynamic::sffz: return mx::api::MarkType::sffz;
    case classify::dynamics::Dynamic::sfzp: return mx::api::MarkType::sfzp;
    case classify::dynamics::Dynamic::rf: return mx::api::MarkType::rf;
    case classify::dynamics::Dynamic::rfz: return mx::api::MarkType::rfz;
    case classify::dynamics::Dynamic::n: return mx::api::MarkType::n;
END_ENUM_CONVERSION

BEGIN_ENUM_CONVERSION(others::Enclosure::Shape, mx::api::RehearsalEnclosure)
    case others::Enclosure::Shape::NoEnclosure: return mx::api::RehearsalEnclosure::none;
    case others::Enclosure::Shape::Rectangle: return mx::api::RehearsalEnclosure::rectangle;
    case others::Enclosure::Shape::Ellipse: return mx::api::RehearsalEnclosure::oval;
    case others::Enclosure::Shape::Triangle: return mx::api::RehearsalEnclosure::triangle;
    case others::Enclosure::Shape::Diamond: return mx::api::RehearsalEnclosure::diamond;
    case others::Enclosure::Shape::Pentagon: return mx::api::RehearsalEnclosure::square;
    case others::Enclosure::Shape::Hexagon: return mx::api::RehearsalEnclosure::square;
    case others::Enclosure::Shape::Heptagon: return mx::api::RehearsalEnclosure::square;
    case others::Enclosure::Shape::Octogon: return mx::api::RehearsalEnclosure::square;
END_ENUM_CONVERSION

BEGIN_ENUM_CONVERSION(classify::articulation::Fermata::Shape, mx::api::MarkType)
    case classify::articulation::Fermata::Shape::Angled: return mx::api::MarkType::fermataAngled;
    case classify::articulation::Fermata::Shape::Curlew: return mx::api::MarkType::fermataCurlew;
    case classify::articulation::Fermata::Shape::DoubleAngled: return mx::api::MarkType::fermataDoubleAngled;
    case classify::articulation::Fermata::Shape::DoubleDot: return mx::api::MarkType::fermataDoubleDot;
    case classify::articulation::Fermata::Shape::DoubleSquare: return mx::api::MarkType::fermataDoubleSquare;
    case classify::articulation::Fermata::Shape::HalfCurve: return mx::api::MarkType::fermataHalfCurve;
    case classify::articulation::Fermata::Shape::Normal: return mx::api::MarkType::fermataNormal;
    case classify::articulation::Fermata::Shape::Square: return mx::api::MarkType::fermataSquare;
END_ENUM_CONVERSION

BEGIN_ENUM_CONVERSION(MusicXmlPitchContext, KeySignature::KeyContext)
    case MusicXmlPitchContext::Concert: return KeySignature::KeyContext::Concert;
    case MusicXmlPitchContext::Written: return KeySignature::KeyContext::Written;
END_ENUM_CONVERSION

BEGIN_ENUM_CONVERSION(music_theory::NoteName, mx::api::Step)
    case music_theory::NoteName::A: return mx::api::Step::a;
    case music_theory::NoteName::B: return mx::api::Step::b;
    case music_theory::NoteName::C: return mx::api::Step::c;
    case music_theory::NoteName::D: return mx::api::Step::d;
    case music_theory::NoteName::E: return mx::api::Step::e;
    case music_theory::NoteName::F: return mx::api::Step::f;
    case music_theory::NoteName::G: return mx::api::Step::g;
END_ENUM_CONVERSION

BEGIN_ENUM_CONVERSION(NoteType, mx::api::DurationName)
    case NoteType::Maxima: return mx::api::DurationName::maxima;
    case NoteType::Longa: return mx::api::DurationName::longa;
    case NoteType::Breve: return mx::api::DurationName::breve;
    case NoteType::Whole: return mx::api::DurationName::whole;
    case NoteType::Half: return mx::api::DurationName::half;
    case NoteType::Quarter: return mx::api::DurationName::quarter;
    case NoteType::Eighth: return mx::api::DurationName::eighth;
    case NoteType::Note16th: return mx::api::DurationName::dur16th;
    case NoteType::Note32nd: return mx::api::DurationName::dur32nd;
    case NoteType::Note64th: return mx::api::DurationName::dur64th;
    case NoteType::Note128th: return mx::api::DurationName::dur128th;
    case NoteType::Note256th: return mx::api::DurationName::dur256th;
    case NoteType::Note512th: return mx::api::DurationName::dur512th;
    case NoteType::Note1024th: return mx::api::DurationName::dur1024th;
    default: break; // causes a throw
END_ENUM_CONVERSION

BEGIN_ENUM_CONVERSION(classify::articulation::Ornament::Type, mx::api::MarkType)
    case classify::articulation::Ornament::Type::InvertedMordent: return mx::api::MarkType::invertedMordent;
    case classify::articulation::Ornament::Type::InvertedTurn: return mx::api::MarkType::invertedTurn;
    case classify::articulation::Ornament::Type::Mordent: return mx::api::MarkType::mordent;
    case classify::articulation::Ornament::Type::Shake: return mx::api::MarkType::shake;
    case classify::articulation::Ornament::Type::Trill: return mx::api::MarkType::trillMark;
    case classify::articulation::Ornament::Type::Turn: return mx::api::MarkType::turn;
END_ENUM_CONVERSION

BEGIN_ENUM_CONVERSION(classify::notehead::Shape, mx::api::Notehead)
    case classify::notehead::Shape::Regular: return mx::api::Notehead::normal;
    case classify::notehead::Shape::X: return mx::api::Notehead::x;
    case classify::notehead::Shape::Diamond: return mx::api::Notehead::diamond;
    case classify::notehead::Shape::SmallSlash: return mx::api::Notehead::slash;
    case classify::notehead::Shape::LargeSlash: return mx::api::Notehead::slash;
    case classify::notehead::Shape::Circle: return mx::api::Notehead::circled;
    case classify::notehead::Shape::Other: return mx::api::Notehead::other;
    case classify::notehead::Shape::Unclassified: break; // causes a throw; callers must not convert an unclassified shape
END_ENUM_CONVERSION

BEGIN_ENUM_CONVERSION(others::SmartShape::ShapeType, mx::api::OttavaType)
    case others::SmartShape::ShapeType::OctaveDown: return mx::api::OttavaType::o8vb;
    case others::SmartShape::ShapeType::OctaveUp: return mx::api::OttavaType::o8va;
    case others::SmartShape::ShapeType::TwoOctaveDown: return mx::api::OttavaType::o15mb;
    case others::SmartShape::ShapeType::TwoOctaveUp: return mx::api::OttavaType::o15ma;
    default: break; // causes a throw
END_ENUM_CONVERSION

BEGIN_ENUM_CONVERSION(details::StaffGroup::DrawBarlineStyle, mx::api::GroupBarline)
    case details::StaffGroup::DrawBarlineStyle::OnlyOnStaves: return mx::api::GroupBarline::no;
    case details::StaffGroup::DrawBarlineStyle::ThroughStaves: return mx::api::GroupBarline::yes;
    case details::StaffGroup::DrawBarlineStyle::Mensurstriche: return mx::api::GroupBarline::mensurstrich;
END_ENUM_CONVERSION

BEGIN_ENUM_CONVERSION(classify::articulation::TechniqueMark::Type, mx::api::MarkType)
    case classify::articulation::TechniqueMark::Type::BrassBend: return mx::api::MarkType::brassBend;
    case classify::articulation::TechniqueMark::Type::BrassFlip: return mx::api::MarkType::flip;
    case classify::articulation::TechniqueMark::Type::BrassHalfMuted: return mx::api::MarkType::halfMuted;
    case classify::articulation::TechniqueMark::Type::BrassLift: return mx::api::MarkType::unspecified;
    case classify::articulation::TechniqueMark::Type::BrassOpen: return mx::api::MarkType::open;
    case classify::articulation::TechniqueMark::Type::BrassSmear: return mx::api::MarkType::smear;
    case classify::articulation::TechniqueMark::Type::BrassStopped: return mx::api::MarkType::stopped;
    case classify::articulation::TechniqueMark::Type::BuzzPizzicato: return mx::api::MarkType::unspecified;
    case classify::articulation::TechniqueMark::Type::Fingernails: return mx::api::MarkType::fingernails;
    case classify::articulation::TechniqueMark::Type::LeftHandPizzicato: return mx::api::MarkType::unspecified;
    case classify::articulation::TechniqueMark::Type::SnapPizzicato: return mx::api::MarkType::snapPizzicato;
    case classify::articulation::TechniqueMark::Type::ThumbPosition: return mx::api::MarkType::thumbPosition;
    case classify::articulation::TechniqueMark::Type::UpBow: return mx::api::MarkType::upBow;
    case classify::articulation::TechniqueMark::Type::DownBow: return mx::api::MarkType::downBow;
    case classify::articulation::TechniqueMark::Type::StringHarmonic: return mx::api::MarkType::harmonic;
END_ENUM_CONVERSION

BEGIN_ENUM_CONVERSION(others::TextBlock::TextJustify, mx::api::HorizontalAlignment)
    case others::TextBlock::TextJustify::Left: return mx::api::HorizontalAlignment::left;
    case others::TextBlock::TextJustify::Center: return mx::api::HorizontalAlignment::center;
    case others::TextBlock::TextJustify::Right: return mx::api::HorizontalAlignment::right;
    case others::TextBlock::TextJustify::Full: return mx::api::HorizontalAlignment::unspecified;
    case others::TextBlock::TextJustify::ForcedFull: return mx::api::HorizontalAlignment::unspecified;
END_ENUM_CONVERSION

BEGIN_ENUM_CONVERSION(VerticalPlacement, mx::api::Placement)
    case VerticalPlacement::NotApplicable: return mx::api::Placement::unspecified;
    case VerticalPlacement::Float: return mx::api::Placement::unspecified;
    case VerticalPlacement::Above: return mx::api::Placement::above;
    case VerticalPlacement::Below: return mx::api::Placement::below;
END_ENUM_CONVERSION

} // namespace detail
} // namespace musicxml
} // namespace formats
} // namespace denigma
