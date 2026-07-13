#include "MidiFileWriter.h"

namespace MidiFileWriter
{
    namespace
    {
        // Duplicated verbatim from Source/UI/ChordNameFormatter.h's kNoteNames
        // (06-RESEARCH.md's explicit guidance: duplicate the 12-entry array
        // here rather than refactor the shared header this phase).
        constexpr const char* kNoteNames[12] =
            { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

        // Zero/negative-length notes are dropped, not written (Pitfall 5):
        // some hosts silently discard a zero-tick-length note anyway, and a
        // defensive guard here is cheaper than relying on that.
        juce::MidiMessageSequence toNoteSequence (const std::vector<NoteEvent>& notes, int tpqn)
        {
            juce::MidiMessageSequence seq;
            for (const auto& n : notes)
            {
                if (n.lengthBeats <= 0.0)
                    continue;

                const double onTick  = n.startBeats * (double) tpqn;
                const double offTick = (n.startBeats + n.lengthBeats) * (double) tpqn;

                seq.addEvent (juce::MidiMessage::noteOn  (1, n.pitch, n.velocity), onTick);
                seq.addEvent (juce::MidiMessage::noteOff (1, n.pitch, 0.0f),        offTick);
            }
            seq.updateMatchedPairs();
            seq.sort();
            return seq;
        }
    }

    juce::MidiFile buildMidiFile (const MidiSetRow& row, double bpm)
    {
        const double safeBpm = bpm > 0.0 ? bpm : 120.0;

        juce::MidiFile file;
        file.setTicksPerQuarterNote (kTicksPerQuarterNote); // MUST be set before addTrack

        // Track 0: tempo + time signature only (format-1 convention -- keeps
        // meta-events off the note track, the more broadly-conventional SMF
        // shape for drag-imported "MIDI pack"-style files).
        juce::MidiMessageSequence metaTrack;
        metaTrack.addEvent (juce::MidiMessage::tempoMetaEvent (juce::roundToInt (60000000.0 / safeBpm)), 0.0);
        metaTrack.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0.0); // v1: 4/4 only,
            // matches AnalysisResult.barStartBeatIndices' existing "every 4th beat" 4/4 assumption
        metaTrack.addEvent (juce::MidiMessage::endOfTrack(), 0.0);
        file.addTrack (metaTrack);

        // Track 1: the row's notes.
        auto noteTrack = toNoteSequence (row.notes, kTicksPerQuarterNote);
        noteTrack.addEvent (juce::MidiMessage::endOfTrack(), noteTrack.getEndTime() /*already in ticks*/);
        file.addTrack (noteTrack);

        return file;
    }

    bool writeToFile (const MidiSetRow& row, double bpm, const juce::File& destination)
    {
        if (! destination.getParentDirectory().createDirectory())
            return false; // no-op if it already exists; false only on real failure

        juce::TemporaryFile temp (destination); // atomic replace -- see Pitfall 4
        {
            juce::FileOutputStream stream (temp.getFile());
            if (! stream.openedOk())
                return false;

            auto midiFile = buildMidiFile (row, bpm);
            if (! midiFile.writeTo (stream, 1)) // format 1
                return false;
        }
        return temp.overwriteTargetFileWithTemporary();
    }

    juce::String suggestedFileName (const MidiSetRow& row, const KeyResult& key, double bpm)
    {
        // row.id is already a filesystem-safe slug -- never use row.label
        // (contains '/', '&', spaces).
        const juce::String keyLabel = juce::String (kNoteNames[key.tonicPitchClass]) + (key.isMajor ? "" : "m");
        const int bpmInt = juce::roundToInt (bpm > 0.0 ? bpm : 120.0);
        return row.id + "_" + keyLabel + "_" + juce::String (bpmInt) + "bpm.mid";
    }
}
