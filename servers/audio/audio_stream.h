/*************************************************************************/
/*  audio_stream.h                                                       */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2022 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2022 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#ifndef AUDIO_STREAM_H
#define AUDIO_STREAM_H

#include "core/io/image.h"
#include "core/io/resource.h"
#include "servers/audio/audio_filter_sw.h"
#include "servers/audio_server.h"

#include "core/object/gdvirtual.gen.inc"
#include "core/object/script_language.h"
#include "core/variant/native_ptr.h"

class AudioStreamPlayback : public RefCounted {
	GDCLASS(AudioStreamPlayback, RefCounted);

protected:
	static void _bind_methods();
	GDVIRTUAL1(_start, float)
	GDVIRTUAL0(_stop)
	GDVIRTUAL0RC(bool, _is_playing)
	GDVIRTUAL0RC(int, _get_loop_count)
	GDVIRTUAL0RC(float, _get_playback_position)
	GDVIRTUAL1(_seek, float)
	GDVIRTUAL3R(int, _mix, GDNativePtr<AudioFrame>, float, int)
public:
	virtual void start(float p_from_pos = 0.0);
	virtual void stop();
	virtual bool is_playing() const;

	virtual int get_loop_count() const; //times it looped

	virtual float get_playback_position() const;
	virtual void seek(float p_time);

	virtual int mix(AudioFrame *p_buffer, float p_rate_scale, int p_frames);
};

class AudioStreamPlaybackResampled : public AudioStreamPlayback {
	GDCLASS(AudioStreamPlaybackResampled, AudioStreamPlayback);

	enum {
		FP_BITS = 16, //fixed point used for resampling
		FP_LEN = (1 << FP_BITS),
		FP_MASK = FP_LEN - 1,
		INTERNAL_BUFFER_LEN = 256,
		CUBIC_INTERP_HISTORY = 4
	};

	AudioFrame internal_buffer[INTERNAL_BUFFER_LEN + CUBIC_INTERP_HISTORY];
	unsigned int internal_buffer_end = -1;
	uint64_t mix_offset = 0;

protected:
	void begin_resample();
	// Returns the number of frames that were mixed.
	virtual int _mix_internal(AudioFrame *p_buffer, int p_frames);
	virtual float get_stream_sampling_rate();

	GDVIRTUAL2R(int, _mix_resampled, GDNativePtr<AudioFrame>, int)
	GDVIRTUAL0RC(float, _get_stream_sampling_rate)

	static void _bind_methods();

public:
	virtual int mix(AudioFrame *p_buffer, float p_rate_scale, int p_frames) override;

	AudioStreamPlaybackResampled() { mix_offset = 0; }
};

class AudioStream : public Resource {
	GDCLASS(AudioStream, Resource);
	OBJ_SAVE_TYPE(AudioStream); // Saves derived classes with common type so they can be interchanged.

protected:
	static void _bind_methods();

	GDVIRTUAL0RC(Ref<AudioStreamPlayback>, _instance_playback)
	GDVIRTUAL0RC(String, _get_stream_name)
	GDVIRTUAL0RC(float, _get_length)
	GDVIRTUAL0RC(bool, _is_monophonic)

public:
	virtual Ref<AudioStreamPlayback> instance_playback();
	virtual String get_stream_name() const;

	virtual float get_length() const;
	virtual bool is_monophonic() const;
};

// Microphone

class AudioStreamPlaybackMicrophone;

class AudioStreamMicrophone : public AudioStream {
	GDCLASS(AudioStreamMicrophone, AudioStream);
	friend class AudioStreamPlaybackMicrophone;

	Set<AudioStreamPlaybackMicrophone *> playbacks;

protected:
	static void _bind_methods();

public:
	virtual Ref<AudioStreamPlayback> instance_playback() override;
	virtual String get_stream_name() const override;

	virtual float get_length() const override; //if supported, otherwise return 0

	virtual bool is_monophonic() const override;

	AudioStreamMicrophone();
};

class AudioStreamPlaybackMicrophone : public AudioStreamPlaybackResampled {
	GDCLASS(AudioStreamPlaybackMicrophone, AudioStreamPlaybackResampled);
	friend class AudioStreamMicrophone;

	bool active = false;
	unsigned int input_ofs = 0;

	Ref<AudioStreamMicrophone> microphone;

protected:
	virtual int _mix_internal(AudioFrame *p_buffer, int p_frames) override;
	virtual float get_stream_sampling_rate() override;

public:
	virtual int mix(AudioFrame *p_buffer, float p_rate_scale, int p_frames) override;

	virtual void start(float p_from_pos = 0.0) override;
	virtual void stop() override;
	virtual bool is_playing() const override;

	virtual int get_loop_count() const override; //times it looped

	virtual float get_playback_position() const override;
	virtual void seek(float p_time) override;

	~AudioStreamPlaybackMicrophone();
	AudioStreamPlaybackMicrophone();
};

//

class AudioStreamPlaybackRandomizer;

class AudioStreamRandomizer : public AudioStream {
	GDCLASS(AudioStreamRandomizer, AudioStream);

public:
	enum PlaybackMode {
		PLAYBACK_RANDOM_NO_REPEATS,
		PLAYBACK_RANDOM,
		PLAYBACK_SEQUENTIAL,
	};

private:
	friend class AudioStreamPlaybackRandomizer;

	struct PoolEntry {
		Ref<AudioStream> stream;
		float weight;
	};

	Set<AudioStreamPlaybackRandomizer *> playbacks;
	Vector<PoolEntry> audio_stream_pool;
	float random_pitch_scale = 1.1f;
	float random_volume_offset_db = 5.0f;

	Ref<AudioStreamPlayback> instance_playback_random();
	Ref<AudioStreamPlayback> instance_playback_no_repeats();
	Ref<AudioStreamPlayback> instance_playback_sequential();

	Ref<AudioStream> last_playback = nullptr;
	PlaybackMode playback_mode = PLAYBACK_RANDOM_NO_REPEATS;

protected:
	static void _bind_methods();

	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

public:
	void add_stream(int p_index);
	void move_stream(int p_index_from, int p_index_to);
	void remove_stream(int p_index);

	void set_stream(int p_index, Ref<AudioStream> p_stream);
	Ref<AudioStream> get_stream(int p_index) const;
	void set_stream_probability_weight(int p_index, float p_weight);
	float get_stream_probability_weight(int p_index) const;

	void set_streams_count(int p_count);
	int get_streams_count() const;

	void set_random_pitch(float p_pitch_scale);
	float get_random_pitch() const;

	void set_random_volume_offset_db(float p_volume_offset_db);
	float get_random_volume_offset_db() const;

	void set_playback_mode(PlaybackMode p_playback_mode);
	PlaybackMode get_playback_mode() const;

	virtual Ref<AudioStreamPlayback> instance_playback() override;
	virtual String get_stream_name() const override;

	virtual float get_length() const override; //if supported, otherwise return 0
	virtual bool is_monophonic() const override;

	AudioStreamRandomizer();
};

class AudioStreamPlaybackRandomizer : public AudioStreamPlayback {
	GDCLASS(AudioStreamPlaybackRandomizer, AudioStreamPlayback);
	friend class AudioStreamRandomizer;

	Ref<AudioStreamRandomizer> randomizer;
	Ref<AudioStreamPlayback> playback;
	Ref<AudioStreamPlayback> playing;

	float pitch_scale;
	float volume_scale;

public:
	virtual void start(float p_from_pos = 0.0) override;
	virtual void stop() override;
	virtual bool is_playing() const override;

	virtual int get_loop_count() const override; //times it looped

	virtual float get_playback_position() const override;
	virtual void seek(float p_time) override;

	virtual int mix(AudioFrame *p_buffer, float p_rate_scale, int p_frames) override;

	~AudioStreamPlaybackRandomizer();
};

VARIANT_ENUM_CAST(AudioStreamRandomizer::PlaybackMode);

#endif // AUDIO_STREAM_H
