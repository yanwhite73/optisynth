// Canonical continuous FX schema. Order/IDs are append-only once shipped.
#pragma once
#include <array>
#include <cmath>
#include <string_view>
namespace partials {
enum class ControlScale { Linear, Logarithmic, ZeroSeconds, ZeroMilliseconds };
struct EffectControlSpec {
    const char *type, *tag, *key, *label, *unit;
    float minimum, maximum, initial;
    ControlScale scale;
    bool manualOnly;
};
constexpr int kEffectControls=71;
inline constexpr std::array<EffectControlSpec,kEffectControls> effectControlSpecs{{
    {"reverb","Rv","predelay_ms","PreDelay","ms",0.0f,2000.0f,25.0f,ControlScale::ZeroMilliseconds,false},
    {"reverb","Rv","time_s","Time","s",0.1f,50.0f,2.5f,ControlScale::Logarithmic,false},
    {"reverb","Rv","damping","Damp","",0.0f,0.95f,0.15f,ControlScale::Linear,false},
    {"reverb","Rv","mod_depth","Mod Depth","",0.0f,20.0f,10.0f,ControlScale::Linear,false},
    {"delay_stereo","SD","time_l_ms","Time L","ms",1.0f,8000.0f,300.0f,ControlScale::Logarithmic,false},
    {"delay_stereo","SD","time_r_ms","Time R","ms",1.0f,8000.0f,450.0f,ControlScale::Logarithmic,false},
    {"delay_stereo","SD","feedback","Feedback","",0.0f,0.99f,0.4f,ControlScale::Linear,false},
    {"delay_stereo","SD","highpass_hz","HP","Hz",8.0f,20480.0f,20.0f,ControlScale::Logarithmic,false},
    {"delay_stereo","SD","lowpass_hz","LP","Hz",20.0f,22000.0f,20480.0f,ControlScale::Logarithmic,false},
    {"delay_stereo","SD","stereo","Stereo","",-1.0f,1.0f,0.0f,ControlScale::Linear,false},
    {"delay_mod","MD","time_ms","Time","ms",0.01f,8000.0f,300.0f,ControlScale::Logarithmic,false},
    {"delay_mod","MD","feedback","Feedback","",0.0f,0.99f,0.4f,ControlScale::Linear,false},
    {"delay_mod","MD","mod_depth","Mod Depth","",0.0f,1.0f,0.0f,ControlScale::Linear,false},
    {"delay_mod","MD","mod_rate_hz","Mod Rate","Hz",0.01f,20.0f,0.5f,ControlScale::Logarithmic,false},
    {"delay_mod","MD","stereo","Stereo","",-1.0f,1.0f,0.0f,ControlScale::Linear,false},
    {"delay_mod","MD","highpass_hz","HP","Hz",8.0f,20480.0f,20.0f,ControlScale::Logarithmic,false},
    {"delay_mod","MD","lowpass_hz","LP","Hz",20.0f,22000.0f,20480.0f,ControlScale::Logarithmic,false},
    {"delay_pingpong","PD","time_l_ms","Time L","ms",1.0f,8000.0f,250.0f,ControlScale::Logarithmic,false},
    {"delay_pingpong","PD","time_r_ms","Time R","ms",1.0f,8000.0f,250.0f,ControlScale::Logarithmic,false},
    {"delay_pingpong","PD","time_c_ms","PreDelay","ms",0.0f,8000.0f,0.0f,ControlScale::ZeroMilliseconds,false},
    {"delay_pingpong","PD","feedback","Feedback","",0.0f,0.99f,0.4f,ControlScale::Linear,false},
    {"delay_pingpong","PD","balance","Balance","",0.0f,1.0f,0.5f,ControlScale::Linear,true},
    {"delay_pingpong","PD","pan_l","Pan L","",-1.0f,1.0f,-1.0f,ControlScale::Linear,true},
    {"delay_pingpong","PD","pan_r","Pan R","",-1.0f,1.0f,1.0f,ControlScale::Linear,true},
    {"chorus","Ch","time_ms","Time","ms",0.01f,40.0f,8.0f,ControlScale::Logarithmic,false},
    {"chorus","Ch","rate_hz","Rate","Hz",0.001f,10.0f,0.5f,ControlScale::Logarithmic,false},
    {"chorus","Ch","rate_spread","Spread","",0.0f,1.0f,0.3f,ControlScale::Linear,false},
    {"chorus","Ch","stereo","Stereo","",0.0f,1.0f,0.5f,ControlScale::Linear,false},
    {"phaser","Ph","center_hz","Center","Hz",50.0f,12000.0f,800.0f,ControlScale::Logarithmic,false},
    {"phaser","Ph","depth","Depth","",0.0f,1.0f,0.5f,ControlScale::Linear,false},
    {"phaser","Ph","feedback","Feedback","",0.0f,0.95f,0.0f,ControlScale::Linear,false},
    {"phaser","Ph","stereo","Stereo","",0.0f,1.0f,0.5f,ControlScale::Linear,false},
    {"phaser","Ph","rate_hz","Rate","Hz",0.01f,20.0f,0.3f,ControlScale::Logarithmic,false},
    {"flanger","Fl","time_ms","Time","ms",0.01f,85.0f,2.0f,ControlScale::Logarithmic,false},
    {"flanger","Fl","time_depth","Depth","",0.0f,1.0f,0.5f,ControlScale::Linear,false},
    {"flanger","Fl","feedback","Feedback","",0.0f,0.999f,0.5f,ControlScale::Linear,false},
    {"flanger","Fl","pan_depth","Pan Depth","",0.0f,1.0f,0.0f,ControlScale::Linear,true},
    {"flanger","Fl","stereo","Stereo","",0.0f,1.0f,0.5f,ControlScale::Linear,false},
    {"flanger","Fl","rate_hz","Rate","Hz",0.001f,20.0f,0.2f,ControlScale::Logarithmic,false},
    {"filter","Flt","cutoff_hz","Cutoff","Hz",20.0f,20480.0f,2000.0f,ControlScale::Logarithmic,false},
    {"filter","Flt","resonance","Resonance","",0.0f,1.0f,0.0f,ControlScale::Linear,false},
    {"filter","Flt","saturation","Sat","",0.0f,1.0f,0.0f,ControlScale::Linear,false},
    {"filter","Flt","mod_rate_hz","Mod Rate","Hz",0.01f,320.0f,0.5f,ControlScale::Logarithmic,false},
    {"filter","Flt","mod_depth","Mod Depth","",0.0f,1.0f,0.0f,ControlScale::Linear,false},
    {"filter","Flt","level_db","Level","dB",-60.0f,24.0f,0.0f,ControlScale::Linear,true},
    {"compressor","Comp","ratio","Ratio",":1",1.0f,40.0f,2.0f,ControlScale::Logarithmic,false},
    {"compressor","Comp","threshold_db","Thresh","dB",-60.0f,0.0f,-12.0f,ControlScale::Linear,false},
    {"compressor","Comp","attack_ms","Attack","ms",0.0f,1000.0f,10.0f,ControlScale::ZeroMilliseconds,false},
    {"compressor","Comp","release_ms","Release","ms",1.0f,5000.0f,200.0f,ControlScale::Logarithmic,false},
    {"compressor","Comp","out_db","Output","dB",-60.0f,24.0f,0.0f,ControlScale::Linear,true},
    {"crusher","Cr","bits","Bits","bit",1.0f,16.01f,16.0f,ControlScale::Linear,false},
    {"crusher","Cr","divider","Divider","",1.0f,32.0f,1.0f,ControlScale::Logarithmic,false},
    {"crusher","Cr","cutoff_hz","Cutoff","Hz",20.0f,20480.0f,20480.0f,ControlScale::Logarithmic,false},
    {"crusher","Cr","resonance","Resonance","",0.0f,1.0f,0.0f,ControlScale::Linear,false},
    {"gator","Gt","smooth","Smooth","",0.0f,1.0f,0.1f,ControlScale::Linear,false},
    {"eq","EQ","freq1_hz","Freq 1","Hz",20.0f,20480.0f,500.0f,ControlScale::Logarithmic,false},
    {"eq","EQ","gain1_db","Gain 1","dB",-24.0f,24.0f,0.0f,ControlScale::Linear,false},
    {"eq","EQ","q1","Q 1","",0.08f,12.5f,1.0f,ControlScale::Logarithmic,false},
    {"eq","EQ","freq2_hz","Freq 2","Hz",20.0f,20480.0f,2000.0f,ControlScale::Logarithmic,false},
    {"eq","EQ","gain2_db","Gain 2","dB",-24.0f,24.0f,0.0f,ControlScale::Linear,false},
    {"eq","EQ","q2","Q 2","",0.08f,12.5f,1.0f,ControlScale::Logarithmic,false},
    {"eq","EQ","low_hz","Low Freq","Hz",20.0f,1000.0f,100.0f,ControlScale::Logarithmic,false},
    {"eq","EQ","low_db","Low Gain","dB",-24.0f,24.0f,0.0f,ControlScale::Linear,false},
    {"eq","EQ","high_hz","High Freq","Hz",440.0f,11000.0f,8000.0f,ControlScale::Logarithmic,false},
    {"eq","EQ","high_db","High Gain","dB",-24.0f,24.0f,0.0f,ControlScale::Linear,false},
    {"wah","Wah","cutoff_hz","Cutoff","Hz",20.0f,20480.0f,400.0f,ControlScale::Logarithmic,false},
    {"wah","Wah","resonance","Resonance","",0.0f,1.0f,0.5f,ControlScale::Linear,false},
    {"wah","Wah","sensitivity","Sens","",-1.0f,1.0f,0.01f,ControlScale::Linear,false},
    {"wah","Wah","attack_s","Attack","s",0.001f,8.0f,0.01f,ControlScale::Logarithmic,false},
    {"wah","Wah","release_s","Release","s",0.001f,8.0f,0.1f,ControlScale::Logarithmic,false},
    {"wah","Wah","level","Level","",0.0f,10.0f,1.0f,ControlScale::Linear,true},
}};
inline int effectControlOffset(std::string_view type) {
    for(int i=0;i<kEffectControls;++i)if(type==effectControlSpecs[i].type)return i;
    return -1;
}
namespace fxparam {
namespace reverb { enum { predelay_ms, time_s, damping, mod_depth }; }
namespace delay_stereo { enum { time_l_ms, time_r_ms, feedback, highpass_hz, lowpass_hz, stereo }; }
namespace delay_mod { enum { time_ms, feedback, mod_depth, mod_rate_hz, stereo, highpass_hz, lowpass_hz }; }
namespace delay_pingpong { enum { time_l_ms, time_r_ms, time_c_ms, feedback, balance, pan_l, pan_r }; }
namespace chorus { enum { time_ms, rate_hz, rate_spread, stereo }; }
namespace phaser { enum { center_hz, depth, feedback, stereo, rate_hz }; }
namespace flanger { enum { time_ms, time_depth, feedback, pan_depth, stereo, rate_hz }; }
namespace filter { enum { cutoff_hz, resonance, saturation, mod_rate_hz, mod_depth, level_db }; }
namespace compressor { enum { ratio, threshold_db, attack_ms, release_ms, out_db }; }
namespace crusher { enum { bits, divider, cutoff_hz, resonance }; }
namespace gator { enum { smooth }; }
namespace eq { enum { freq1_hz, gain1_db, q1, freq2_hz, gain2_db, q2, low_hz, low_db, high_hz, high_db }; }
namespace wah { enum { cutoff_hz, resonance, sensitivity, attack_s, release_s, level }; }
}
} // namespace partials
