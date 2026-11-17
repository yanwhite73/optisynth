// Included inside test_core's anonymous namespace, after the shared helpers.
void testHostExtension() {
    for(float gain:{-12.f,12.f}){
        auto eq=makeEffect("eq");eq->prepare(48000,{{"wet",1},{"freq1_hz",500},{"gain1_db",gain},{"q1",1.5f}});
        std::array<float,256> l{},r{};double input=0,output=0;
        for(int block=0;block<128;++block){
            for(int n=0;n<256;++n){l[n]=r[n]=.01f*std::sin(2*3.141592653589793*500*(block*256+n)/48000.);if(block>=100)input+=l[n]*l[n];}
            eq->process(l.data(),r.data(),256);
            if(block>=100)for(float x:l)output+=x*x;
        }
        check(std::abs(10*std::log10(output/input)-gain)<.002,"moving-coefficient EQ retains static band-centre gain");
    }
    Envelope envelope;EnvelopeParams e;e.attack=1;e.sustain=.8f;e.release=.1f;
    envelope.start(e);envelope.advance(.3f);const float before=envelope.level();
    e.attack=2;e.attackCurve=20;envelope.updateParams(e);
    check(envelope.level()==before,"live attack/curve edit preserves current level");
    const float after=envelope.advance(.0001f);
    check(std::isfinite(after)&&std::abs(after-before)<.001f,"extreme live curve remains finite and continuous");
    for(int i=0;i<3000;++i)envelope.advance(.001f);
    e.sustain=.2f;envelope.updateParams(e);
    for(int i=0;i<100;++i)envelope.advance(.001f);
    check(std::abs(envelope.level()-.2f)<1e-5,"held sustain responds to live control");
    envelope.noteOff();for(int i=0;i<200;++i)envelope.advance(.001f);
    e.sustain=1;e.release=60;envelope.updateParams(e);
    check(envelope.finished()&&envelope.advance(.01f)==0,"envelope edits do not resurrect released notes");

    auto patch=defaultPatch();patch.mainEnv.release=.1f;
    patch.matrix[0]={"constant","osc1.pitch",0};
    Engine a(48000),b(48000);a.load(patch,{},true);b.load(patch,a.tableCache(),true);
    a.noteOn({60,.8f});b.noteOn({60,.8f});
    std::array<float,256> al{},ar{},bl{},br{};
    a.render(al.data(),ar.data(),256);b.render(bl.data(),br.data(),256);
    auto values=liveValues(patch);values[kLiveMatrixStart]=.15f;
    realtimeTest::allocations=0;realtimeTest::tracking=true;
    b.setLiveControls(values);double difference=0;
    for(int block=0;block<40;++block){
        a.render(al.data(),ar.data(),256);b.render(bl.data(),br.data(),256);
        for(int n=0;n<256;++n)difference+=std::abs(al[n]-bl[n]);
    }
    realtimeTest::tracking=false;
    check(realtimeTest::allocations==0,"expanded engine controls/render allocate no memory");
    check(difference>1&&b.activeVoices()==1,"zero-depth matrix route responds during a held note");
    values[kLiveEnvStart+3]=.1f;b.setLiveControls(values);
    double quiet=0,loud=0;
    for(int block=0;block<40;++block){a.render(al.data(),ar.data(),256);b.render(bl.data(),br.data(),256);
        if(block>20)for(int n=0;n<256;++n){loud+=al[n]*al[n];quiet+=bl[n]*bl[n];}}
    check(quiet<loud*.1,"amplitude envelope host control changes held-note audio");

    // Every detailed FX control must produce observable output in a configured
    // instance. Both copies have identical history before the live edit.
    for(int index=0;index<kEffectControls;++index){
        const auto& spec=effectControlSpecs[index];const int offset=effectControlOffset(spec.type);
        EffectParams params{{"wet",1},{"modulation",1},{"mod_depth",.6f},{"depth",.6f},
            {"gain1_db",6},{"gain2_db",6},{"low_db",6},{"high_db",6},{"sensitivity",.03f}};
        if(std::string_view(spec.type)=="crusher")params["cutoff_hz"]=3000; // resonance needs its filter enabled
        std::array<float,10> initial{},changed{};int count=0;
        for(int i=offset;i<kEffectControls&&std::string_view(effectControlSpecs[i].type)==spec.type;++i){
            const auto& s=effectControlSpecs[i];auto found=params.find(s.key);
            initial[i-offset]=found==params.end()?s.initial:found->second;++count;
        }
        changed=initial;
        const float n=initial[index-offset]<(spec.minimum+spec.maximum)*.5f?.73f:.27f;
        changed[index-offset]=spec.minimum+n*(spec.maximum-spec.minimum);
        auto reference=makeEffect(spec.type),edited=makeEffect(spec.type);
        reference->prepare(48000,params);edited->prepare(48000,params);
        double delta=0;bool finite=true;
        for(int block=0;block<260;++block){
            for(int sample=0;sample<256;++sample){
                const double t=(block*256+sample)/48000.;
                const float gate=std::fmod(t,.18)<.09?1.f:.08f;
                al[sample]=bl[sample]=gate*static_cast<float>(.5*std::sin(1729*t)+.2*std::sin(23711*t));
                ar[sample]=br[sample]=gate*static_cast<float>(.4*std::sin(2891*t)+.15*std::sin(18171*t));
            }
            realtimeTest::allocations=0;realtimeTest::tracking=true;
            if(block==20)edited->updateControls(changed.data());
            reference->process(al.data(),ar.data(),256);edited->process(bl.data(),br.data(),256);
            realtimeTest::tracking=false;
            finite&=realtimeTest::allocations==0;
            if(block>20)for(int n=0;n<256;++n){delta+=std::abs(al[n]-bl[n])+std::abs(ar[n]-br[n]);finite&=std::isfinite(bl[n])&&std::isfinite(br[n]);}
        }
        check(finite&&delta>1e-6,std::string("audible live FX control: ")+spec.type+"."+spec.key);
        // Endpoint sweep preserves histories and remains finite.
        for(int block=0;index==offset&&block<400;++block){
            for(int i=0;i<count;++i){const auto& s=effectControlSpecs[offset+i];
                const float target=(block/40)%2?s.minimum:s.maximum;
                changed[i]+=.08f*(target-changed[i]);}
            edited->updateControls(changed.data());al.fill(.01f);ar.fill(-.01f);
            edited->process(al.data(),ar.data(),16);
            for(int n=0;n<16;++n)finite&=std::isfinite(al[n])&&std::isfinite(ar[n]);
        }
        check(finite,std::string("finite FX range endpoints: ")+spec.type+"."+spec.key);
    }
}
