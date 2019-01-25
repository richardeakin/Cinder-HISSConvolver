/*
 Copyright (c) 2018, Richard Eakin - All rights reserved.

 Redistribution and use in source and binary forms, with or without modification, are permitted provided
 that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice, this list of conditions and
 the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
 the following disclaimer in the documentation and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include "cinder/Cinder.h"
#include "cinder/audio/Node.h"
#include "cinder/audio/Source.h"

extern "C" {
	struct t_multi_channel_convolve;
}

// TODO: rename this to ant namespace
namespace mason { namespace audio {

using ConvolverNodeRef = std::shared_ptr<class ConvolverNode>;

class ConvolverNode : public ci::audio::Node {
  public:
	enum class LatencyMode {
		ZERO,
		SHORT,
		MEDIUM
	};

	ConvolverNode( const Format &format = Format() );

	// TODO: update docs
	//! Loads and stores a reference to a Buffer created from the entire contents of \a sourceFile. Resets the loop points to 0:getNumFrames()).
	void loadBuffer( const ci::audio::SourceFileRef &sourceFile );
	//! Sets the current Buffer. Safe to do while enabled. Resets the loop points to 0:getNumFrames()).
	void setBuffer( const ci::audio::BufferRef &buffer );
	//! returns a shared_ptr to the current Buffer.
	const ci::audio::BufferRef& getBuffer() const	{ return mIRBuffer; }

	void setLatencyMode( LatencyMode mode );
	LatencyMode	getLatencyMode() const	{ return mLatencyMode; }

	//!
	void setGainPre( float gainLinear )	{ mGainPreConvolve = gainLinear; }
	//!
	float getGainPreConvolver() const	{ return mGainPreConvolve; }

	void reset();

  protected:
	void initialize() override;
	void uninitialize() override;
	void process( ci::audio::Buffer *buffer ) override;

  private:
	void setIRBufferImpl( const ci::audio::BufferRef &buffer );

	t_multi_channel_convolve*	mMulti;
	ci::audio::BufferRef		mIRBuffer;
	std::vector<float *>		mChannelPointers;
	size_t						mFixedImpulseLength = 0; // zero means dynamic memory allocation for convolvers
	LatencyMode					mLatencyMode = LatencyMode::ZERO;
	std::atomic<float>			mGainPreConvolve = { 1 }; // we're currently calculating what this should be in max, but can be set by hand. Usually low, ~ -50db
};

} } // namespace mason::audio
