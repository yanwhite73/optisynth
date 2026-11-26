#pragma once
#include "Panel.h"

namespace {
class DrumStrokeMap final : public juce::Component, private Timer {
    struct Cell final : TextButton, FileDragAndDropTarget {
        std::function<void(const File&)> drop;
        bool isInterestedInFileDrag(const StringArray& paths) override {return paths.size()==1&&paths[0].endsWithIgnoreCase(".wav");}
        void filesDropped(const StringArray& paths,int,int) override {if(isInterestedInFileDrag(paths)&&drop)drop(File(paths[0]));}
    };
    std::array<Cell,30> cells;
    std::function<String(int)> pathFor;
    std::function<void(int)> choose;
    std::function<void(int,const File&)> assign;
    String title;
    void timerCallback() override {
        for(int i=0;i<30;++i){
            const auto path=pathFor(i);
            auto name=path.isEmpty()?String("Model"):File(path).getFileNameWithoutExtension();
            if(name.startsWith("FRD_FrameDrums_"))name="Skin "+name.fromLastOccurrenceOf("_",false,false);
            cells[i].setButtonText(name);
            cells[i].setTooltip(path.isEmpty()?"Synthesized contact. Click to assign or drop a WAV.":path);
            cells[i].setColour(TextButton::buttonColourId,path.isEmpty()?Colour(0xff383b39):Colour(0xff345e52));
        }
    }
public:
    DrumStrokeMap(String instrument,std::function<String(int)> get,std::function<void(int)> pick,
                  std::function<void(int,const File&)> set)
        :pathFor(std::move(get)),choose(std::move(pick)),assign(std::move(set)),title(instrument+"  /  Stroke map"){
        for(int i=0;i<30;++i){
            auto& cell=cells[i];addAndMakeVisible(cell);
            cell.drop=[this,i](const File& file){assign(i,file);};
            cell.onClick=[this,i]{
                if(pathFor(i).isEmpty()){choose(i);return;}
                PopupMenu menu;menu.addItem(1,"Replace WAV…");menu.addItem(2,"Use synthesis");
                menu.showMenuAsync(PopupMenu::Options().withTargetComponent(&cells[i]),
                    [safe=SafePointer<DrumStrokeMap>(this),i](int action){
                        if(safe&&action==1)safe->choose(i);
                        if(safe&&action==2)safe->assign(i,{});
                    });
            };
        }
        setSize(560,261);timerCallback();startTimerHz(3);
    }
    void paint(Graphics& g) override {
        g.fillAll(Colour(0xff252725));g.setColour(paper);g.setFont(font(16).boldened());
        g.drawText(title,12,5,530,24,Justification::centredLeft);
        g.setFont(font(13).boldened());
        for(int z=0;z<2;++z)g.drawText(z?"Outer skin":"Inner skin",102+z*220,32,212,18,Justification::centred);
        static const char* dynamics[]={"Soft","Medium","Hard"};
        g.setFont(font(11));
        for(int c=0;c<6;++c)g.drawText(dynamics[c%3],102+(c/3)*220+(c%3)*72,54,68,15,Justification::centred);
        static const char* names[]={"C   Bass","D   Open","E   Slap","F   Stopped","G   Fingers"};
        g.setFont(font(13));for(int r=0;r<5;++r)g.drawText(names[r],12,73+r*29,88,25,Justification::centredLeft);
        g.setFont(font(12));g.drawText("Drop a WAV on a layer, or click to choose. Recorded skin sets the blend.",12,226,538,25,Justification::centredLeft);
    }
    void resized() override {
        for(int n=0;n<30;++n){const int c=n%6;
            cells[n].setBounds(102+(c/3)*220+(c%3)*72,73+(n/6)*29,69,25);}
    }
};
}
