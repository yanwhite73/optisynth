#pragma once

void spectralChecks(){
    using namespace partials;
    auto p=defaultPatch();p.name="Stereo qualification";p.osc[0].filter.pan=-.8f;
    p.osc[1]=p.osc[0];p.osc[1].on=true;p.osc[1].semitone=7;p.osc[1].filter.pan=.8f;
    p.lfo[0].rateHz=3;p.mainEnv.release=.2f;
    GraphSpec g;g.nodes={{"engine","spectral.engine",{}}};g.outputs={{"engine","out"}};
    SpectralBank bank{{"engine",spectralPreset(p)}};
    const auto preparationStart=std::chrono::steady_clock::now();
    Runtime r(g,{},bank);auto ready=r.spectralBank();
    const double coldMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-preparationStart).count();
    check(ready.at("engine")->tables[0]&&ready.at("engine")->tables[1],"Spectral active tables prepared");
    Engine direct(48000);auto values=liveValues(p);applyLiveValues(p,values);direct.load(p,ready.at("engine")->tables,true);
    direct.noteOn({48,.8f});
    std::vector<float> left(4800),right(4800),a(4800),b(4800);
    allocations=0;watchAllocations=true;
    for(int offset=0;offset<4800;offset+=quantum){r.render(left.data()+offset,right.data()+offset,quantum,{48,.8f,true,offset==0});direct.render(a.data()+offset,b.data()+offset,quantum);}
    watchAllocations=false;
    check(allocations==0,"Complete Spectral rendering has no allocation");
    check(left==a&&right==b,"Spectral adapter equals direct engine in both channels");
    check(left!=right&&energy(left)>0&&energy(right)>0,"Independent stereo channels remain audible");
    const auto snapshot=direct.modulationSnapshot();check(std::abs(snapshot[0])>.01&&snapshot[5]>0,"Actual voice LFO/envelope exported");
    direct.reset();check(direct.modulationSnapshot()==ModulationSnapshot{},"No active voice exports zero");
    auto dormant=g;dormant.nodes.push_back({"unused","spectral.engine",{}});Runtime pruned(dormant,{},ready);
    check(!pruned.controls("unused")&&pruned.spectralBank().size()==1,"Dormant Spectral has no processor or tables");
    auto copy=g;copy.nodes.push_back({"second","spectral.engine",{}});copy.outputs.push_back({"second","out"});auto twoBank=ready;twoBank["second"]=ready.at("engine");Runtime two(copy,{},twoBank);
    check(two.controls("engine")!=two.controls("second"),"Whole engine controls independently owned");
    check(two.spectralBank().at("engine")->tables[0]==two.spectralBank().at("second")->tables[0],"Compatible engine instances reuse immutable tables");
    copy.nodes.push_back({"third","spectral.engine",{}});copy.outputs.push_back({"third","out"});bool rejected=false;
    try{Runtime excessive(copy);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"Spectral instance bound checked before construction");
    auto mono=g;mono.nodes.push_back({"mono","utility.downmix",{}});mono.connections={{{"engine","out"},{"mono","in"}}};mono.outputs={{"mono","out"}};Runtime mixed(mono,{},ready);
    mixed.render(a.data(),b.data(),4800,{48,.8f,true,true});bool mean=true;for(size_t i=0;i<a.size();++i)mean&=a[i]==.5f*(left[i]+right[i]);
    check(mean&&a==b,"Explicit stereo-to-mono adapter averages L/R");
    auto invalid=g;invalid.nodes.push_back({"filter","mutable.lowpass",{}});invalid.connections={{{"engine","out"},{"filter","in"}}};invalid.outputs={{"filter","out"}};rejected=false;
    try{validateRack(invalid);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"Implicit stereo collapse rejected");
    auto stereo=rackPreset(0);Runtime old(stereo);old.render(a.data(),b.data(),4800,{48,.8f,true,true});check(a==b,"Existing mono graph duplicates identically to L/R");

    auto mod=instrumentPatch("osc.sine","spectral.adsr","filter.bypass",false,false);mod.nodes.push_back({"lfo","mod.lfo",{{"rate",2}}});
    mod.modulations={{{"lfo","out"},{"source","level"},0}};Runtime neutral(mod);auto baseline=render(instrumentPatch("osc.sine","spectral.adsr","filter.bypass",false,false));
    auto unchanged=render(mod);check(unchanged==baseline,"Zero-depth routing preserves baseline audio");
    check(neutral.controls("lfo")!=nullptr,"Control-only dependencies are reachable even at zero depth");
    neutral.routeControls()->amounts[0]=.4f;allocations=0;watchAllocations=true;neutral.render(a.data(),b.data(),4800,{48,.8f,true,true});watchAllocations=false;
    check(allocations==0&&neutral.controls("source")->values[1].load()==.5f,"External modulation leaves saved base unchanged without allocation");
    mod.modulations[0].amount=.4f;check(render(mod)!=baseline,"Independent LFO audibly changes destination");
    mod.modulations.push_back({{"lfo","out"},{"lfo","depth"},.2f});rejected=false;
    try{validateRack(mod);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"Modulation feedback cycle rejected");
    mod.modulations.pop_back();mod.modulations[0].destination.port="missing";rejected=false;
    try{validateRack(mod);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"Unknown parameter route rejected");
    mod.modulations[0]={{"source","out"},{"lfo","depth"},.2f};rejected=false;
    try{validateRack(mod);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"Audio output cannot be an untyped modulation source");
    auto cross=g;cross.nodes.push_back({"source","osc.sine",{}});cross.modulations={{{"engine","mainenv"},{"source","level"},.2f}};cross.outputs={{"source","out"}};
    Runtime exported(cross,{},ready);check(exported.activeModules()==2,"Spectral modulation export retains its source dependency");
    exported.render(a.data(),b.data(),4800,{48,.8f,true,true});check(energy(a)>0,"Actual Spectral envelope drives another module");
    for(double rate:{44100.,48000.,96000.,192000.}){
        Player player;player.publish(std::make_unique<Runtime>(g,SampleBank{},ready));player.note(48,true);OutputConverter converter;converter.prepare(rate);
        allocations=0;watchAllocations=true;converter.render(player,a.data(),b.data(),4800);watchAllocations=false;
        check(a!=b&&energy(a)>0&&energy(b)>0&&allocations==0,"Stereo player/converter preserved at every device rate without allocation");
    }
    check(!spectralParameterActive(p,24)&&!spectralParameterActive(p,54),"Disabled oscillator and FX omitted from candidate destinations");
    check(spectralParameterActive(p,5)&&spectralParameterActive(p,6),"Pan/level remain usable with spectral filter bypassed");
    p.osc[0].filter.on=true;p.osc[0].filter.xmod[0].type=1;
    check(!spectralParameterActive(p,10),"Unimplemented filter RM is not a destination");
    p.osc[0].route1=1;p.fx[0].on=true;p.fx[0].type="delay_mod";p.fx[0].params={{"sync_beats",1},{"wet",.3f}};
    int time=partials::effectControlOffset("delay_mod");
    check(!spectralParameterActive(p,partials::kLiveFxStart+time),"Clocked delay free time is not a destination");
    auto full=spectralDemoPatch();full.arp.mode=2;full.arp.length=4;full.arp.resolutionBeats=.125f;
    full.osc[0].route1=1;full.osc[0].route2=1;full.fx[0].type="delay_stereo";full.fx[0].on=true;full.fx[0].params={{"wet",.3f},{"time_l_ms",30},{"time_r_ms",45}};
    Runtime instrument(g,{},{{"engine",spectralPreset(full)}});auto fullValues=liveValues(full);applyLiveValues(full,fullValues);
    Engine reference(48000);reference.load(full,instrument.spectralBank().at("engine")->tables,true);reference.noteOn({48,.8f});
    const auto start=std::chrono::steady_clock::now();double delta=0;
    for(int block=0;block<4000;++block){instrument.render(a.data(),b.data(),quantum,{48,.8f,block<3000,block==0});if(block==3000)reference.noteOff(48);reference.render(left.data(),right.data(),quantum);
        for(int i=0;i<quantum;++i)delta+=std::abs(a[i]-left[i])+std::abs(b[i]-right[i]);}
    check(delta==0,"Filtered unison preset with internal arp, delay and release matches core");
    const auto seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    std::printf("Spectral: simple cold prepare %.2f ms; full filtered/unison/arp/delay adapter + reference, 2 seconds each in %.3f s (not a dense-polyphony benchmark)\n",coldMs,seconds);
}
