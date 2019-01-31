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

#include "ConvolverNode.h"
#include "cinder/audio/Context.h"
#include "cinder/audio/dsp/Dsp.h"
#include "cinder/Log.h"
#include "cinder/CinderMath.h"

#include "HISSTools_Multichannel_Convolution/multi_channel_convolve.h"

using namespace ci;
using namespace std;

namespace hiss {

ConvolverNode::ConvolverNode( const Format &format )
	: Node( format ), mMulti( nullptr )
{

}

void ConvolverNode::initialize()
{
	auto numChannels = getNumChannels();
	size_t maxLength = 16384;

	t_convolve_latency_mode latencyMode = CONVOLVE_LATENCY_ZERO;
	switch( mLatencyMode ) {
		case LatencyMode::ZERO:
			latencyMode = CONVOLVE_LATENCY_ZERO;
			break;
		case LatencyMode::SHORT:
			latencyMode = CONVOLVE_LATENCY_SHORT;
			break;
		case LatencyMode::MEDIUM:
			latencyMode = CONVOLVE_LATENCY_MEDIUM;
			break;
		default: 
			CI_ASSERT_NOT_REACHABLE();
	}

	mMulti = multi_channel_convolve_new( (AH_UIntPtr)numChannels, (AH_UIntPtr)numChannels, latencyMode, (AH_UIntPtr)maxLength );

	mChannelPointers.resize( numChannels );
	if( mIRBuffer ) {
		// re-initialize the impulse response, since num channels might have changed
		setIRBufferImpl( mIRBuffer );
	}
	CI_LOG_I( "complete. num channels: " << numChannels );
}

void ConvolverNode::uninitialize()
{
	multi_channel_convolve_free( mMulti );
	mMulti = nullptr;
}

void ConvolverNode::reset()
{
	if( ! mMulti )
		return;

	lock_guard<mutex> lock( getContext()->getMutex() );

	multi_channel_convolve_clear( mMulti, mFixedImpulseLength ? false : true );
}

void ConvolverNode::setLatencyMode( LatencyMode mode )
{
	if( mLatencyMode == mode )
		return;

	auto thisRef = shared_from_this();
	getContext()->uninitializeNode( thisRef );
	mLatencyMode = mode;
	getContext()->initializeNode( thisRef );
}

void ConvolverNode::setBuffer( const ci::audio::BufferRef &buffer )
{
	lock_guard<mutex> lock( getContext()->getMutex() );
	setIRBufferImpl( buffer );
}

void ConvolverNode::loadBuffer( const ci::audio::SourceFileRef &sourceFile )
{
	size_t sampleRate = getSampleRate();
	if( sampleRate == sourceFile->getSampleRate() )
		setBuffer( sourceFile->loadBuffer() );
	else {
		auto sf = sourceFile->cloneWithSampleRate( sampleRate );
		setBuffer( sf->loadBuffer() );
	}
}

void ConvolverNode::setIRBufferImpl( const ci::audio::BufferRef &buffer )
{
	mIRBuffer = buffer;
	if( ! mMulti )
		return; // we'll do the multi_channel_convolve_set when initialize() is called by the graph

	AH_UIntPtr impulseLength = (AH_UIntPtr)buffer->getNumFrames();

	CI_LOG_I( "loading IR buffer of length: " << impulseLength << ", channels: " << buffer->getNumChannels() );

	// channels are zero-indexed
	AH_UIntPtr numChannels = glm::min<size_t>( getNumChannels(), buffer->getNumChannels() );
	for( AH_UIntPtr ch = 0; ch < numChannels; ch++ ) {
		float *input			   = buffer->getChannel( ch );
		t_convolve_error set_error = multi_channel_convolve_set( mMulti, ch, ch, input, impulseLength, mFixedImpulseLength ? false : true );
		CI_VERIFY( set_error == CONVOLVE_ERR_NONE );
	}

	CI_LOG_I( "complete." );
}

void ConvolverNode::process( ci::audio::Buffer *buffer )
{
	if( ! mIRBuffer )
		return;

	size_t numChannels = buffer->getNumChannels();
	size_t numFrames = buffer->getNumFrames();

	CI_ASSERT( numChannels == mChannelPointers.size() );

	ci::audio::dsp::mul( buffer->getData(), mGainPreConvolve, buffer->getData(), buffer->getSize() );

	for( size_t ch = 0; ch < numChannels; ch++ ) {
		mChannelPointers[ch] = buffer->getChannel( ch );
	}

	multi_channel_convolve_process_float( mMulti, mChannelPointers.data(), mChannelPointers.data(), 0, 0, (AH_UIntPtr)numFrames, (AH_UIntPtr)numChannels, (AH_UIntPtr)numChannels );
}

} // namespace hiss