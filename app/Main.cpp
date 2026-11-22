#include "Panel.h"
#include "RackEditor.h"
namespace {
class App final : public JUCEApplication {
public:
    const String getApplicationName() override {return "Optisynth";}
    const String getApplicationVersion() override {return "0.0.20";}
    void initialise(const String& args) override {
        // Native checks keep their settings and preset library inside the snapshot folder,
        // starting empty each run; the user's own files are never touched.
        const bool snapshot=args.startsWith("--snapshot ");
        const File snapshotFolder=snapshot?File(args.fromFirstOccurrenceOf("--snapshot ",false,false).unquoted()):File();
        if(snapshot){snapshotFolder.getChildFile("settings").deleteRecursively();snapshotFolder.getChildFile("library").deleteRecursively();}
        const File settings=snapshot?snapshotFolder.getChildFile("settings/Optisynth.settings")
            :File::getSpecialLocation(File::userApplicationDataDirectory).getChildFile("Application Support/Optisynth/Optisynth.settings");
        // Presets root, scanned recursively; user presets are saved into Presets/User (request 73).
        const File library=snapshot?snapshotFolder.getChildFile("library"):File::getSpecialLocation(File::userMusicDirectory).getChildFile("Optisynth/Presets");
        window=std::make_unique<Window>(getApplicationName()+" "+getApplicationVersion(),settings,library);
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
        Window(String name,const File& settings,const File& library):DocumentWindow(name,Colour(0xff1c282d),DocumentWindow::allButtons) {
            setUsingNativeTitleBar(true);synth=new RackSynth(settings,library);setContentOwned(synth,true);setResizable(true,false);setResizeLimits(1120,800,2560,1600);centreWithSize(getWidth(),getHeight());setVisible(true);synth->grabKeyboardFocus();
        }
        void closeButtonPressed() override {JUCEApplication::getInstance()->systemRequestedQuit();}
    };
    std::unique_ptr<Window> window;
};
}
START_JUCE_APPLICATION(App)
