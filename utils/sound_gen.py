#!/usr/bin/env python3

#  Copyright 2022 Nicolas Maltais
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.

# Utility to convert MIDI file to format supported by the game console.
# The format is described in include/core/sound.h
#
# This is just a stripped down version of the utility at: https://github.com/maltaisn/buzzer-midi
# with some name changes to homogenize with the other encoding utilities.
#
# Usage:
# $ ./sound_gen.py <input file> <output file> [options]
# $ ./sound_gen.py <input file> - [options] > output.dat
# $ ./sound_gen.py --help
#
import argparse
import math
import sys
from collections import Counter
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, Set
from typing import List, Optional, Tuple

from mido import MidiFile

# =============================

FramesNotes = List[List[List[int]]]
MidiEventMap = Dict[int, List[Tuple[int, any]]]
MidiTempoMap = Dict[int, int]


def get_note_freq(note: int) -> float:
    """Return the frequency of a note value in Hz (C2=0)."""
    return 440 * 2 ** ((note - 33) / 12)


class EncodeError(Exception):
    pass


@dataclass
class NoteData:
    # note as encoded in sound data, C2 is 0, B7 is 71
    note: int
    # duration, 0-MAX_DURATION (0 being 1/16th of a beat)
    duration: int

    NONE = 0x54
    MAX_DURATION = 0x3fff
    MAX_DURATION_REPEAT = 0x40
    MAX_NOTE = 0x53

    IMMEDIATE_PAUSE_OFFSET = 0x55
    SHORT_PAUSE_OFFSET = 0xaa

    # minimum note resolution for buzzer sound system (1/16th of a beat).
    TIMEFRAME_RESOLUTION = 16

    def encode_duration(self) -> bytes:
        """Encode the duration of a note in one or two bytes."""
        b = bytearray()
        if self.duration < 128:
            b.append(self.duration)
        elif self.duration <= NoteData.MAX_DURATION:
            b.append((self.duration >> 8) | 0xc0)
            b.append(self.duration & 0xff)
        else:
            raise ValueError("cannot encode duration")
        return b

    @staticmethod
    def from_midi(midi_note: int):
        # C2=36 in MIDI, C2=0 in buzzer sound
        return midi_note - 36

    def __repr__(self) -> str:
        note_str = "OFF" if self.note == NoteData.NONE else self.note
        return f"BuzzerNote(note={note_str}, duration={self.duration})"


@dataclass
class TrackData:
    # channel number
    channel: int
    # track notes
    notes: List[NoteData]

    TRACK_NOTES_END = 0xff

    TIMER_PERIOD = 5_000_000
    NOTE_RANGE = range(0, 72)

    def __init__(self, number: int):
        self.channel = number
        self.notes = []

    def add_note(self, note: int) -> None:
        """append note at the end of track, merge with previous note if identical"""
        if note != NoteData.NONE and note not in TrackData.NOTE_RANGE:
            raise ValueError("Note out of range for track")
        if len(self.notes) > 0 and self.notes[-1].note == note and \
                self.notes[-1].duration < NoteData.MAX_DURATION:
            self.notes[-1].duration += 1
        else:
            # different note, or previous note exceeded max duration.
            self.notes.append(NoteData(note, 0))

    def finalize(self) -> None:
        """do final modifications on track notes"""
        # remove last 'none' notes if any
        while len(self.notes) > 0 and self.notes[-1].note == NoteData.NONE:
            del self.notes[-1]

    def encode(self) -> bytes:
        last_duration = -1
        duration_repeat = 0
        first_repeat_index = -1
        last_note_index = -1

        b = bytearray()
        b.append(self.channel)
        b += b"\x00\x00"  # track length

        # find most common pause duration shorter than 256 for track and store it
        # it will be used for notes using the immediate pause encoding.
        immediate_pause = -1
        b.append(0)
        if self.notes:
            pause_durations = (note.duration for note in self.notes
                               if note.note == NoteData.NONE and note.duration <= 0xff)
            most_common_pauses = Counter(pause_durations).most_common()
            if most_common_pauses:
                immediate_pause = most_common_pauses[0][0]
                b[-1] = immediate_pause

        def end_duration_repeat() -> None:
            nonlocal duration_repeat, first_repeat_index
            if duration_repeat > 0:
                b[first_repeat_index] = 0x80 | (duration_repeat - 1)
                duration_repeat = 0
                first_repeat_index = -1

        for note in self.notes:
            # append note byte
            if note.note == NoteData.NONE:
                if note.duration == immediate_pause and last_note_index != -1:
                    # note in range [0x55, 0xa8] indicate that note is followed by a pause.
                    b[last_note_index] += NoteData.IMMEDIATE_PAUSE_OFFSET
                    last_note_index = -1
                    continue
                elif note.duration <= (0xff - NoteData.SHORT_PAUSE_OFFSET):
                    # note in range [0xaa, 0xfe] indicate a pause of duration (note - 170).
                    b.append(note.duration + NoteData.SHORT_PAUSE_OFFSET)
                    continue
                elif 128 < note.duration <= 129 + immediate_pause and immediate_pause <= 128:
                    # 0xa9
                    # will almost never happen but if pause is in a narrow duration range
                    # it can be encoded on 2 bytes instead of 3 by combining with immediate pause
                    b.append(NoteData.NONE + NoteData.IMMEDIATE_PAUSE_OFFSET)
                    note = NoteData(note.note, note.duration - immediate_pause - 1)
                else:
                    # 0x54: normal pause
                    last_note_index = len(b)
                    b.append(NoteData.NONE)
            else:
                # note in range [0, 84[ indicate only a note.
                last_note_index = len(b)
                b.append(note.note)

            # append duration
            if note.duration == last_duration:
                # same duration as last note, use repeated duration encoding
                if duration_repeat == NoteData.MAX_DURATION_REPEAT:
                    end_duration_repeat()
                if duration_repeat == 0:
                    first_repeat_index = len(b)
                    b.append(0x00)  # to be set later
                duration_repeat += 1
            else:
                end_duration_repeat()
                b += note.encode_duration()
                last_duration = note.duration

        end_duration_repeat()

        b.append(TrackData.TRACK_NOTES_END)

        if len(b) > 0xffff:
            raise RuntimeError(f"track is too long to be encoded ({len(b)} bytes)")
        b[1:3] = len(b).to_bytes(2, "little", signed=False)
        return b


@dataclass
class SoundData:
    tracks: List[TrackData] = field(default_factory=list)

    SIGNATURE = 0xf2

    SOUND_END = 0xff
    CHANNELS_COUNT = 3

    TEMPO_SLICE = 1e6 / 256
    TEMPO_MIN_US = round(256 * TEMPO_SLICE * NoteData.TIMEFRAME_RESOLUTION)
    TEMPO_MAX_US = round(1 * TEMPO_SLICE * NoteData.TIMEFRAME_RESOLUTION)

    def encode(self) -> bytes:
        if len(set(t.channel for t in self.tracks)) != len(self.tracks):
            raise EncodeError("tracks must be unique")

        b = bytearray()
        b.append(SoundData.SIGNATURE)
        for track in self.tracks:
            if len(track.notes) > 0:
                b += track.encode()
        b.append(SoundData.SOUND_END)
        return b


class ClosestTrackStrategy:
    """
    Game console modified: closest strategy almost always produces the smallest sound data size,
    so other strategies were discarded. The whole point of having multiple strategies was to
    ensure that all notes get assigned in at least one to the track when tracks had different note
    ranges, but this isn't the case here, all tracks are the same.

    assign note to the track that is playing or last played the closest note.
    thus, separate hands should be played separatedly no matter the order in MIDI.
    a track currently playing no note is considered the farthest.
    if two tracks are considered equal using this method, use the track with the most notes,
    then the track with the lowest track number.
    """
    merge_midi_tracks: bool

    def __init__(self, merge_midi_tracks: bool):
        self.merge_midi_tracks = merge_midi_tracks

    def create_tracks(self, channels: Set[int], frames_notes: FramesNotes) -> List[TrackData]:
        """Create a list of buzzer tracks by placing the notes in each frame."""
        tracks = {i: TrackData(i) for i in channels}
        # which MIDI track a buzzer track has been set to, or None if none yet.
        midi_track_assignment: List[Optional[int]] = [None] * SoundData.CHANNELS_COUNT

        def filter_tracks(track: TrackData):
            midi_asg = midi_track_assignment[track.channel]
            return (bnote in TrackData.NOTE_RANGE and
                    (self.merge_midi_tracks or midi_asg is None or midi_track == midi_asg))

        for i in range(len(frames_notes[0])):
            # unassigned tracks for current frame
            unassigned_tracks = {channel: track for channel, track in tracks.items()}

            for midi_track, midi_track_notes in enumerate(frames_notes):
                for note in midi_track_notes[i]:
                    bnote = NoteData.from_midi(note)

                    # filter available tracks to keep only tracks which have had no note
                    # assigned yet to them or tracks which have had notes from this MIDI track.
                    # also keep only tracks on which note can be played
                    legal_tracks = list(filter(filter_tracks, unassigned_tracks.values()))
                    if not legal_tracks:
                        raise EncodeError("could not assign note to track")

                    # apply strategy to choose track for note
                    track_num = self.assign_track(legal_tracks, bnote)

                    del unassigned_tracks[track_num]
                    if midi_track_assignment[track_num] is None:
                        # first note assigned to this track, remember which MIDI track
                        midi_track_assignment[track_num] = midi_track

                    tracks[track_num].add_note(bnote)

            # add "none" notes for remaining unassigned tracks
            for track in unassigned_tracks.values():
                track.add_note(NoteData.NONE)

        for track in tracks.values():
            track.finalize()

        # discard tracks with only a single "none" note
        return list(filter(lambda t: len(t.notes) > 0, tracks.values()))

    def assign_track(self, tracks: List[TrackData], bnote: int) -> int:
        if len(tracks[0].notes) == 0:
            # no notes assigned yet, fallback on first fit.
            return tracks[0].channel

        # find track playing note closest to this note
        closest_track: Optional[int] = None
        closest_count = 0
        min_note_dist = 0
        for i, track in enumerate(tracks):
            curr_note = track.notes[-1].note
            if curr_note == NoteData.NONE and len(track.notes) > 1:
                curr_note = track.notes[-2].note
            note_dist = math.inf if curr_note == NoteData.NONE else abs(bnote - curr_note)
            if closest_track is None or note_dist < min_note_dist or \
                    (note_dist == min_note_dist and len(track.notes) > closest_count):
                closest_track = track.channel
                closest_count = len(track.notes)
                min_note_dist = note_dist

        return closest_track


STD_IO = "-"

NOTE_NAMES = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]


def bpm_to_beat_us(bpm: float) -> float:
    """Convert BPM tempo to beat period in microseconds."""
    return 6e7 / bpm


def beat_us_to_bpm(us: float) -> float:
    """Convert BPM tempo to beat period in microseconds."""
    return 6e7 / us


def format_midi_note(note: int) -> str:
    """Convert midi note 0-127 to note name."""
    return f"{NOTE_NAMES[note % 12]}{note // 12 - 1}"


@dataclass
class Config:
    input_file: Path
    output_file: str
    tempo: int
    octave_adjust: int
    merge_midi_tracks: bool
    time_range: Optional[slice]
    channels: Set[int]
    verbose: bool


def create_config(args: argparse.Namespace) -> Config:
    """Validate input arguments and create typed configuration object."""
    input_path = Path(args.input_file)
    if not input_path.exists() or not input_path.is_file():
        raise ValueError("input file doesn't exist")

    # tempo
    tempo_bpm = args.tempo
    tempo_min = math.ceil(beat_us_to_bpm(SoundData.TEMPO_MIN_US))
    tempo_max = math.floor(beat_us_to_bpm(SoundData.TEMPO_MAX_US))
    if not (tempo_min <= tempo_bpm <= tempo_max):
        raise ValueError(f"tempo value out of bounds (between {tempo_min} and {tempo_max} BPM)")
    tempo_us = round(bpm_to_beat_us(tempo_bpm))

    # time range
    time_range: Optional[slice] = None
    if args.time_range:
        parts = args.time_range.split(":")
        if len(parts) != 2:
            raise ValueError("invalid time range")
        try:
            start = float(parts[0]) if parts[0] else 0
            end = float(parts[1]) if parts[1] else None
            time_range = slice(start, end)
        except ValueError:
            raise ValueError("invalid time range")

    # channels
    channels_str = args.channels.split(",")
    channels = set()
    for channel_str in channels_str:
        try:
            channel = int(channel_str.strip())
        except ValueError:
            raise ValueError("invalid channels list")
        if not (0 <= channel < SoundData.CHANNELS_COUNT):
            raise ValueError(f"channel {channel} doesn't exist")
        channels.add(channel)

    verbose = args.output_file != STD_IO

    return Config(args.input_file, args.output_file, tempo_us, args.octave_adjust,
                  args.merge_midi_tracks, time_range, channels, verbose)


class SoundEncoder:
    """Class used to interpret configuration and output data and WAV file for buzzer sound."""
    config: Config

    def __init__(self, config: Config):
        self.config = config

    def encode(self) -> SoundData:
        config = self.config
        midi = MidiFile(config.input_file, clip=True)

        track_count = len(midi.tracks)
        event_map = self._build_event_map(midi)
        self._info(f"Event map built, {len(event_map)} events in {track_count} tracks")

        # get tempo info
        tempo_map = self._get_tempo_map(event_map)
        tempo = config.tempo

        # create note frames for entire duration
        midi_duration = max(event_map.keys())
        midi_duration_sec = midi_duration / midi.ticks_per_beat * tempo / 1e6
        frames = self._get_all_frames(tempo_map, tempo, midi_duration, midi.ticks_per_beat)
        self._info(f"Frames time computed, got {len(frames)} frames")

        # get notes played in each frame, for each MIDI track
        frames_notes = self._get_frame_notes(event_map, frames, track_count)
        frames_notes = self._apply_time_range(frames_notes, tempo, midi_duration_sec)

        # do some validation before applying track assignment strategy
        self._check_max_notes_at_once(frames_notes, tempo)
        self._verify_note_range(frames_notes, tempo)

        # create buzzer sound from frames notes
        sound_data = SoundData()
        track_strategy = ClosestTrackStrategy(self.config.merge_midi_tracks)
        tracks = track_strategy.create_tracks(self.config.channels, frames_notes)
        sound_data.tracks = tracks
        return sound_data

    def _info(self, message: str) -> None:
        if self.config.verbose:
            print(message)

    def _build_event_map(self, midi: MidiFile) -> MidiEventMap:
        """Group midi messages in all the tracks by the time at which they occur
        keep track of the original track number."""
        event_map: MidiEventMap = {}
        for i, track in enumerate(midi.tracks):
            time = 0
            for event in track:
                time += event.time
                if time not in event_map:
                    event_map[time] = []
                event_map[time].append((i, event))
        return event_map

    def _get_tempo_map(self, event_map: MidiEventMap) -> MidiTempoMap:
        """Build MIDI tempo map (tempo in us/beat by MIDI time)."""
        tempo_map: MidiTempoMap = {0: 500000}
        for time, events in event_map.items():
            tempo_event = next((e[1] for e in events if e[1].type == "set_tempo"), None)
            if tempo_event:
                tempo_map[time] = tempo_event.tempo
        return tempo_map

    def _get_average_tempo(self, tempo_map: MidiTempoMap, midi_duration: int) -> float:
        """Get weighted average of tempo in map, by tempo duration."""
        avg_tempo = 0
        curr_tempo = 0
        last_tempo_time = 0
        for time, tempo in sorted(tempo_map.items()):
            avg_tempo += curr_tempo * (time - last_tempo_time)
            curr_tempo = tempo
            last_tempo_time = time
        avg_tempo += curr_tempo * (midi_duration - last_tempo_time)
        avg_tempo /= midi_duration
        return avg_tempo

    def _get_all_frames(self, tempo_map: MidiTempoMap, tempo: float, midi_duration: int,
                        ticks_per_beat: int) -> List[int]:
        """From average tempo, event map and tempo map, compute the MIDI clock for each 1/16th
        of beat, for the duration of the whole file, while accounting for variable tempo."""
        frames: List[int] = []
        midi_ticks = 0.0
        tempo_map_sorted = sorted(tempo_map.items())
        curr_tempo = None
        while midi_ticks < midi_duration:
            if not curr_tempo or tempo_map_sorted and midi_ticks >= tempo_map_sorted[0][0]:
                curr_tempo = tempo_map_sorted[0][1]
                del tempo_map_sorted[0]
            frames.append(round(midi_ticks))
            # advance time to go to next timeframe, using average tempo as reference tempo
            # and taking clocks per tick into account
            midi_ticks += (tempo / curr_tempo) * ticks_per_beat / NoteData.TIMEFRAME_RESOLUTION
        return frames

    def _get_frame_notes(self, event_map: MidiEventMap, frames: List[int],
                         track_count: int) -> FramesNotes:
        """Get all notes being played on each frame and in between frames, for each MIDI track.
        detect when notes go on and off during same frame to prevent omitting notes."""
        timelines: FramesNotes = [[] for _ in range(track_count)]
        notes_on: List[List[int]] = [[] for _ in range(track_count)]
        new_notes_on = set()
        for i, frame in enumerate(frames):
            events = list(event_map.get(frame, []))
            if i != len(frames) - 1:
                # also get all events in between this frame and the next,
                # in order to not miss any events
                for j in range(frame + 1, frames[i + 1]):
                    events += event_map.get(j, [])

            # update list of notes on per track
            for track_num, event in events:
                notes_on_track = notes_on[track_num]
                new_notes_on.clear()
                if event.type == "note_on" or event.type == "note_off":
                    note = event.note + round(self.config.octave_adjust * 12)
                    if event.type == "note_on" and note not in notes_on_track:
                        # start playing note. if note_on event and note is already being played,
                        # interpret as note_off (?).
                        notes_on_track.append(note)
                        new_notes_on.add(note)
                    elif note in notes_on_track:
                        notes_on_track.remove(note)

            # save notes for frame
            for j, timeline in enumerate(timelines):
                timeline.append(list(notes_on[j]))

        return timelines

    def _apply_time_range(self, frames_notes: FramesNotes,
                          tempo: float, midi_duration_sec: float) -> FramesNotes:
        if self.config.time_range:
            nframes = len(frames_notes[0])
            start = self.config.time_range.start
            end = self.config.time_range.stop

            if start <= -midi_duration_sec:
                raise EncodeError("invalid time slice: bad start time")
            elif start < 0:
                start += midi_duration_sec

            if end is None:
                end = midi_duration_sec
            elif end <= -midi_duration_sec:
                raise EncodeError("invalid time slice: bad end time")
            elif end < 0:
                end += midi_duration_sec

            time_per_frame = tempo / (NoteData.TIMEFRAME_RESOLUTION * 1e6)
            frame_first = round(start / time_per_frame)
            frame_last = round(end / time_per_frame) + 1
            if frame_last < frame_first:
                raise EncodeError(f"invalid time slice with end time before start time")
            if frame_first > nframes:
                raise EncodeError(f"invalid time slice starting after file end")
            if frame_last > nframes:
                frame_last = nframes
            self._info(f"Time slice from {start:.1f} s to {end:.1f} s, "
                       f"keeping {frame_last - frame_first} frames")
            return [notes[frame_first:frame_last] for notes in frames_notes]
        return frames_notes

    def _check_max_notes_at_once(self, frames_notes: FramesNotes, tempo: float) -> None:
        """Check if maximum number of notes played at once in all tracks combined is
        less or equal to the number of channels."""
        nframes = len(frames_notes[0])
        notes_per_frame = [sum(len(notes[i]) for notes in frames_notes) for i in range(nframes)]
        max_notes = max(notes_per_frame)
        if max_notes > SoundData.CHANNELS_COUNT:
            # more notes played at once than channels available.
            # give some info on time of occurence in file.
            time = (notes_per_frame.index(max_notes) /
                    (NoteData.TIMEFRAME_RESOLUTION * 1e6) * tempo)
            raise EncodeError(f"can't convert, up to {max_notes} notes played at once "
                              f"(at around {time:.1f} s, only {SoundData.CHANNELS_COUNT} "
                              f"channels available)")
        else:
            self._info(f"File has at most {max_notes} notes played at once")

    def _verify_note_range(self, frames_notes: FramesNotes, tempo: float) -> None:
        """Check that no note in file exceeds the largest timer range and
        give some information on notes and timing if bad notes found."""
        for track_notes in frames_notes:
            for i, frame_notes in enumerate(track_notes):
                for j, note in enumerate(frame_notes):
                    if not NoteData.from_midi(note) in TrackData.NOTE_RANGE:
                        # bad note, give some info on it
                        # given time is approximate since based on overall tempo.
                        time = i / (NoteData.TIMEFRAME_RESOLUTION * 1e6) * tempo
                        raise EncodeError(f"can't convert, found note {format_midi_note(note)} "
                                          f"exceeding timer range (at around {time:.1f} s)")

    def _create_sound_data(self, tempo: float, frames_notes: FramesNotes) -> SoundData:
        """Create sound data from frames notes using specified strategy."""
        # create empty sound data with set tempo


def create_sound_data(config: Config) -> SoundData:
    return SoundEncoder(config).encode()


parser = argparse.ArgumentParser(
    description="Sound encoding utility for game console, from MIDI files",
    formatter_class=argparse.RawTextHelpFormatter)
parser.add_argument("input_file", type=str, help="Input MIDI file")
parser.add_argument("output_file", type=str, default=STD_IO, nargs="?",
                    help=f"Output file. Can be set to '{STD_IO}' for standard output (default).")
parser.add_argument("-t", "--tempo", type=int, required=True,
                    help="Tempo in BPM. Note that this only affects the encoded tempo, not the "
                         "actual tempo of the sound. The tempo is the same for all tracks so "
                         "this should be set to a common value.", dest="tempo")
parser.add_argument("-r", "--range", action="store", type=str,
                    help="Time range to use from MIDI file, in seconds. Default is whole file.\n"
                         "- ':10': first 10 seconds\n"
                         "- '-10:': last 10 seconds\n"
                         "- '10:': all except first 10 seconds\n"
                         "- ':-10': all except last 10 seconds\n"
                         "- '10:-10': between first 10 s and last 10 s",
                    dest="time_range")
parser.add_argument("-m", "--merge-tracks", action="store_true",
                    help="Merge MIDI tracks when creating buzzer tracks",
                    dest="merge_midi_tracks")
parser.add_argument("-o", "--octave", type=float, help="Octave adjustment for whole file",
                    dest="octave_adjust", default=0)
parser.add_argument("-c", "--channels", action="store", type=str, default="0,1,2",
                    help="Channels to use, separated by commas. Default is 0,1,2.",
                    dest="channels")


def main() -> None:
    args = parser.parse_args()
    config = None
    try:
        config = create_config(args)
    except ValueError as e:
        print(f"ERROR: {e}", file=sys.stderr)
        exit(1)

    converter = SoundEncoder(config)
    sound_data = None
    try:
        sound_data = converter.encode()
    except EncodeError as e:
        print(f"ERROR: {e}", file=sys.stderr)
        exit(1)

    # output encoded data
    data = sound_data.encode()
    if config.verbose:
        print(f"Total data size is {len(data)} bytes")
    try:
        if config.output_file == STD_IO:
            sys.stdout.buffer.write(data)
            sys.stdout.flush()
        else:
            with open(config.output_file, "wb") as file:
                file.write(data)
    except IOError as e:
        raise EncodeError(f"could not write to output file: {e}") from e


if __name__ == '__main__':
    main()
