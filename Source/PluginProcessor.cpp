/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
FirstCompressorAudioProcessor::FirstCompressorAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
    using namespace Params;
    const auto params=GetParams();
    auto floathelper=[&apvts=this->apvts, &params](auto& param, const auto& paramName){
        param=dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter(params.at(paramName)));
        jassert(param!=nullptr);
    };
    
    // I wonder if all the repetition here can be avoided. (This looks dogshit, and won't scale to a dynamic number of bands)
    floathelper(lowBandComp.attack, Names::Attack_Low_Band);
    floathelper(lowBandComp.release, Names::Release_Low_Band);
    floathelper(lowBandComp.threshold, Names::Threshold_Low_Band);
    
    floathelper(midBandComp.attack, Names::Attack_Mid_Band);
    floathelper(midBandComp.release, Names::Release_Mid_Band);
    floathelper(midBandComp.threshold, Names::Threshold_Mid_Band);
    
    floathelper(highBandComp.attack, Names::Attack_High_Band);
    floathelper(highBandComp.release, Names::Release_High_Band);
    floathelper(highBandComp.threshold, Names::Threshold_High_Band);
    
    auto choicehelper=[&apvts=this->apvts, &params](auto& param, const auto& paramName){
        param=dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(params.at(paramName)));
        jassert(param!=nullptr);
    };
    
    choicehelper(lowBandComp.ratio, Names::Ratio_Low_Band);
    choicehelper(midBandComp.ratio, Names::Ratio_Mid_Band);
    choicehelper(highBandComp.ratio, Names::Ratio_High_Band);
    
    auto boolhelper=[&apvts=this->apvts, &params](auto& param, const auto& paramName){
        param=dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter(params.at(paramName)));
        jassert(param!=nullptr);
    };
    
    boolhelper(lowBandComp.bypassed, Names::Bypassed_Low_Band);
    boolhelper(midBandComp.bypassed, Names::Bypassed_Mid_Band);
    boolhelper(highBandComp.bypassed, Names::Bypassed_High_Band);
    
    boolhelper(lowBandComp.mute, Names::Mute_Low_Band);
    boolhelper(midBandComp.mute, Names::Mute_Mid_Band);
    boolhelper(highBandComp.mute, Names::Mute_High_Band);
    
    boolhelper(lowBandComp.solo, Names::Solo_Low_Band);
    boolhelper(midBandComp.solo, Names::Solo_Mid_Band);
    boolhelper(highBandComp.solo, Names::Solo_High_Band);
    
    floathelper(lowMidCrossover, Names::Low_Mid_Crossover_Freq);
    floathelper(midHighCrossover, Names::Mid_High_Crossover_Freq);
    
    floathelper(inputGainParam, Names::Gain_In);
    floathelper(outputGainParam, Names::Gain_Out);
    
    LP1.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
    HP1.setType(juce::dsp::LinkwitzRileyFilterType::highpass);
    
    AP2.setType(juce::dsp::LinkwitzRileyFilterType::allpass);
    
    LP2.setType(juce::dsp::LinkwitzRileyFilterType::lowpass);
    HP2.setType(juce::dsp::LinkwitzRileyFilterType::highpass);
    
    
}

FirstCompressorAudioProcessor::~FirstCompressorAudioProcessor()
{
}

//==============================================================================
const juce::String FirstCompressorAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool FirstCompressorAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool FirstCompressorAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool FirstCompressorAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double FirstCompressorAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int FirstCompressorAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int FirstCompressorAudioProcessor::getCurrentProgram()
{
    return 0;
}

void FirstCompressorAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String FirstCompressorAudioProcessor::getProgramName (int index)
{
    return {};
}

void FirstCompressorAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void FirstCompressorAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    
    juce::dsp::ProcessSpec spec;
    spec.maximumBlockSize=samplesPerBlock;
    spec.numChannels=getTotalNumOutputChannels();
    spec.sampleRate=sampleRate;
    
    for (auto& comp:compressors){
        comp.prepare(spec);
    }
    
    LP1.prepare(spec);
    HP1.prepare(spec);
    
    AP2.prepare(spec);
    
    LP2.prepare(spec);
    HP2.prepare(spec);
    
    inputGain.prepare(spec);
    outputGain.prepare(spec);
    
    inputGain.setRampDurationSeconds(0.05);
    outputGain.setRampDurationSeconds(0.05);
    
    for (auto buffer:filterBuffers){
        buffer.setSize(spec.numChannels, samplesPerBlock);
    }
    
}

void FirstCompressorAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool FirstCompressorAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void FirstCompressorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
    
    inputGain.setGainDecibels(inputGainParam->get());
    outputGain.setGainDecibels(outputGainParam->get());
    
    applyGain(buffer,inputGain);
    
//    compressor.updateCompressorSettings();
//    compressor.process(buffer);
    for (auto& compressor : compressors){
        compressor.updateCompressorSettings();
    }
    
    for (auto& fb:filterBuffers){
        fb=buffer;
    }
    
    // FIND A WAY TO D-R-Y THIS OUT
    auto lowMidCutoffFreq=lowMidCrossover->get();
    LP1.setCutoffFrequency(lowMidCutoffFreq);
    HP1.setCutoffFrequency(lowMidCutoffFreq);
    
    auto midHighCutoffFreq=midHighCrossover->get();
    AP2.setCutoffFrequency(midHighCutoffFreq);
    LP2.setCutoffFrequency(midHighCutoffFreq);
    HP2.setCutoffFrequency(midHighCutoffFreq);
    
    
    auto fb0block=juce::dsp::AudioBlock<float>(filterBuffers[0]);
    auto fb1block=juce::dsp::AudioBlock<float>(filterBuffers[1]);
    auto fb2block=juce::dsp::AudioBlock<float>(filterBuffers[2]);
    
    auto fb0ctx=juce::dsp::ProcessContextReplacing<float>(fb0block);
    auto fb1ctx=juce::dsp::ProcessContextReplacing<float>(fb1block);
    auto fb2ctx=juce::dsp::ProcessContextReplacing<float>(fb2block);
    
    LP1.process(fb0ctx);
    AP2.process(fb0ctx);
    HP1.process(fb1ctx);
    filterBuffers[2]=filterBuffers[1];
    LP2.process(fb1ctx);
    HP2.process(fb2ctx);
    
    for (size_t i=0;i<compressors.size();++i){
        compressors[i].process(filterBuffers[i]);
    }
    
    auto numSamples=buffer.getNumSamples();
    auto numChannels=buffer.getNumChannels();
    
    buffer.clear();
    
    auto addFilterBand=[nc=numChannels,ns=numSamples](auto& inputBuffer,const auto& source){
        for (auto i=0;i<nc;++i){
            inputBuffer.addFrom(i, 0, source, i, 0, ns);
        }
    };
    
    auto bandsAreSoloed=false;
    for (auto& comp:compressors){
        if (comp.solo->get()){
            bandsAreSoloed=true;
            break;
        }
    }
    
    
//    addFilterBand(buffer, filterBuffers[0]);
//    addFilterBand(buffer, filterBuffers[1]);
//    addFilterBand(buffer, filterBuffers[2]);
    if (bandsAreSoloed){
        for (size_t i=0;i<compressors.size();i++){
            auto& comp=compressors[i];
            if (comp.solo->get()){
                addFilterBand(buffer, filterBuffers[i]);
            }
        }
    }
    else{
        for (size_t i=0;i<compressors.size();i++){
            auto& comp=compressors[i];
            if (! comp.mute->get()){
                addFilterBand(buffer, filterBuffers[i]);
            }
        }
    }
    
    applyGain(buffer, outputGain);
    
    
}

//==============================================================================
bool FirstCompressorAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* FirstCompressorAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

//==============================================================================
void FirstCompressorAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    
    juce::MemoryOutputStream mos(destData,true);
    apvts.state.writeToStream(mos);
    
}

void FirstCompressorAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    auto tree=juce::ValueTree::readFromData(data, sizeInBytes);
    if (tree.isValid()){
        apvts.replaceState(tree);
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout FirstCompressorAudioProcessor::createParameterLayout(){
    APVTS::ParameterLayout layout;
    
    using namespace juce;
    using namespace Params;
    const auto& params=GetParams();
    
    auto gainRange=NormalisableRange<float>(-24.f, 24.f, .5f, 1.f);
    
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID { params.at(Names::Gain_In), 5 },
                                                     params.at(Names::Gain_In),
                                                     gainRange,
                                                     0));
    
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID { params.at(Names::Gain_Out), 5 },
                                                     params.at(Names::Gain_Out),
                                                     gainRange,
                                                     0));
    
    // CHANGE AND OPTIMIZE ALL OF THIS
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID { params.at(Names::Threshold_Low_Band), 3 },
                                                     params.at(Names::Threshold_Low_Band),
                                                     NormalisableRange<float>(-60, 12, 1, 1),
                                                     0));
    
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID { params.at(Names::Threshold_Mid_Band), 3 },
                                                     params.at(Names::Threshold_Mid_Band),
                                                     NormalisableRange<float>(-60, 12, 1, 1),
                                                     0));
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID { params.at(Names::Threshold_High_Band), 3 },
                                                     params.at(Names::Threshold_High_Band),
                                                     NormalisableRange<float>(-60, 12, 1, 1),
                                                     0));
    
    auto attackReleaseRange=NormalisableRange<float>(5, 500, 1, 1);
    
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID { params.at(Names::Attack_Low_Band) , 3 },
                                                     params.at(Names::Attack_Low_Band),
                                                     attackReleaseRange,
                                                     50));
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID { params.at(Names::Attack_Mid_Band) , 3 },
                                                     params.at(Names::Attack_Mid_Band),
                                                     attackReleaseRange,
                                                     50));
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID { params.at(Names::Attack_High_Band) , 3 },
                                                     params.at(Names::Attack_High_Band),
                                                     attackReleaseRange,
                                                     50));
    
    
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID { params.at(Names::Release_Low_Band), 3 },
                                                     params.at(Names::Release_Low_Band),
                                                     attackReleaseRange,
                                                     250));
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID { params.at(Names::Release_Mid_Band), 3 },
                                                     params.at(Names::Release_Mid_Band),
                                                     attackReleaseRange,
                                                     250));
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID { params.at(Names::Release_High_Band), 3 },
                                                     params.at(Names::Release_High_Band),
                                                     attackReleaseRange,
                                                     250));
    
    auto choices=std::vector<double>{1,1.5,2,3,4,5,6,7,8,10,15,20,50,100};
    juce::StringArray sa;
    for (auto choice:choices){
        sa.add(juce::String(choice,1));
    }
    
    
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID { params.at(Names::Ratio_Low_Band), 3 },
                                                      params.at(Names::Ratio_Low_Band),
                                                      sa,
                                                      3));
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID { params.at(Names::Ratio_Mid_Band), 3 },
                                                      params.at(Names::Ratio_Mid_Band),
                                                      sa,
                                                      3));
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID { params.at(Names::Ratio_High_Band), 3 },
                                                      params.at(Names::Ratio_High_Band),
                                                      sa,
                                                      3));
    
    layout.add(std::make_unique<AudioParameterBool>(ParameterID{params.at(Names::Bypassed_Low_Band),3},
                                                    params.at(Names::Bypassed_Low_Band),
                                                    false));
    layout.add(std::make_unique<AudioParameterBool>(ParameterID{params.at(Names::Bypassed_Mid_Band),3},
                                                    params.at(Names::Bypassed_Mid_Band),
                                                    false));
    layout.add(std::make_unique<AudioParameterBool>(ParameterID{params.at(Names::Bypassed_High_Band),3},
                                                    params.at(Names::Bypassed_High_Band),
                                                    false));
    
    layout.add(std::make_unique<AudioParameterBool>(ParameterID{params.at(Names::Mute_Low_Band),4},
                                                    params.at(Names::Mute_Low_Band),
                                                    false));
    layout.add(std::make_unique<AudioParameterBool>(ParameterID{params.at(Names::Mute_Mid_Band),4},
                                                    params.at(Names::Mute_Mid_Band),
                                                    false));
    layout.add(std::make_unique<AudioParameterBool>(ParameterID{params.at(Names::Mute_High_Band),4},
                                                    params.at(Names::Mute_High_Band),
                                                    false));
    
    layout.add(std::make_unique<AudioParameterBool>(ParameterID{params.at(Names::Solo_Low_Band),4},
                                                    params.at(Names::Solo_Low_Band),
                                                    false));
    layout.add(std::make_unique<AudioParameterBool>(ParameterID{params.at(Names::Solo_Mid_Band),4},
                                                    params.at(Names::Solo_Mid_Band),
                                                    false));
    layout.add(std::make_unique<AudioParameterBool>(ParameterID{params.at(Names::Solo_High_Band),4},
                                                    params.at(Names::Solo_High_Band),
                                                    false));
    
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID{params.at(Names::Low_Mid_Crossover_Freq),3}, params.at(Names::Low_Mid_Crossover_Freq), NormalisableRange<float>(20, 999, 1, 1), 400));
    
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID{params.at(Names::Mid_High_Crossover_Freq),3}, params.at(Names::Mid_High_Crossover_Freq), NormalisableRange<float>(1000, 20000, 1, 1), 2000));
    
    return layout;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FirstCompressorAudioProcessor();
}
