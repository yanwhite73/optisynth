#pragma once
#include <algorithm>
#include <array>

// Local typing-key ownership. UI supplies focus/Caps Lock state and physical keys;
// only notes started by this object are released when typing is disabled.
class TypingKeyboard {
public:
    static constexpr char keys[]="AWSEDFTGYHUJKOLP;'";
    int base=48;
    TypingKeyboard(){notes.fill(-1);}
    template<class Off> void release(Off off) {
        for(auto& n:notes)if(n>=0){off(n);n=-1;}
    }
    template<class On,class Off>
    void update(bool enabled,const std::array<bool,18>& down,bool z,bool x,On on,Off off) {
        if(!enabled){release(off);zWasDown=xWasDown=false;return;}
        if((z&&!zWasDown)||(x&&!xWasDown)){
            release(off);base=std::clamp(base+(x&&!xWasDown?12:-12),0,108);
        }
        zWasDown=z;xWasDown=x;
        for(size_t i=0;i<notes.size();++i){
            if(down[i]&&notes[i]<0){int n=base+(int)i;if(n<=127){notes[i]=n;on(n);}}
            else if(!down[i]&&notes[i]>=0){off(notes[i]);notes[i]=-1;}
        }
    }
private:
    std::array<int,18> notes;
    bool zWasDown=false,xWasDown=false;
};
