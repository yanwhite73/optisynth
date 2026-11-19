#include "Panel.h"
#include "RackEditor.h"
namespace {
class App final : public JUCEApplication {
public:
    const String getApplicationName() override {return "Optisynth";}
    const String getApplicationVersion() override {return "0.0.13";}
    void initialise(const String& args) override {
        window=std::make_unique<Window>(getApplicationName()+" "+getApplicationVersion());
        if(args=="--spectral-demo")window->synth->showSpectralDemo();
        if(args=="--sample-demo")window->synth->showSampleDemo();
        if(args.isEmpty()||args=="--sequencer-demo")window->synth->showReferenceDemo();
        if(args.startsWith("--snapshot ")) {
            const File path(args.fromFirstOccurrenceOf("--snapshot ",false,false).unquoted());
            MessageManager::callAsync([this,path]{try{window->synth->snapshots(path);}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());setApplicationReturnValue(1);}quit();});
        }
    }
    void shutdown() override {window.reset();}
    void systemRequestedQuit() override {quit();}
private:
    struct Window : DocumentWindow {
        RackSynth* synth;
        explicit Window(String name):DocumentWindow(name,Colour(0xff1c282d),DocumentWindow::allButtons) {
            setUsingNativeTitleBar(true);synth=new RackSynth;setContentOwned(synth,true);setResizable(true,false);setResizeLimits(1120,800,1800,1200);centreWithSize(getWidth(),getHeight());setVisible(true);synth->grabKeyboardFocus();
        }
        void closeButtonPressed() override {JUCEApplication::getInstance()->systemRequestedQuit();}
    };
    std::unique_ptr<Window> window;
};
}
START_JUCE_APPLICATION(App)
