// Render the actual registered effect; delay and hold tests use a single impulse.
static void prismChecks() {
    using Settings = std::vector<std::pair<std::string,float>>;
    auto impulse = [](Settings settings, int freezeAt = -1, bool secondStrike = false) {
        auto unit = makeAnalog({"tank","fx.prism_tank",std::move(settings)},{});
        std::vector<float> result(2 * 72000);
        std::array<float,quantum> left{},right{},outL{},outR{};
        const float* inL[]{left.data()}; const float* inR[]{right.data()};
        float* dstL[]{outL.data()}; float* dstR[]{outR.data()};
        allocations = 0;
        watchAllocations = true;
        for(int pos=0;pos<72000;pos+=quantum) {
            if(pos==freezeAt)unit->controls->values[9].store(1);
            left[0] = pos==0 || (secondStrike && pos==36000) ? .7f : 0.f;
            unit->process({quantum,{},inL,dstL,inR,dstR});
            std::copy(outL.begin(),outL.end(),result.begin()+pos);
            std::copy(outR.begin(),outR.end(),result.begin()+72000+pos);
        }
        watchAllocations = false;
        check(allocations==0,"Prism performs without allocation");
        check(std::all_of(result.begin(),result.end(),[](float v){return std::isfinite(v)&&std::abs(v)<4;}),
              "Prism output bounded and finite");
        return result;
    };
    Settings base{{"mix",1},{"predelay",10},{"echo",0},{"pitch",0},{"motion",0},{"decay",.93f}};
    auto normal=impulse(base);
    check(std::all_of(normal.begin(),normal.begin()+480,[](float v){return v==0;}) &&
          energy({normal.begin()+3000,normal.begin()+24000})>1e-5,"Predelay then audible diffused tail");
    check(!std::equal(normal.begin(),normal.begin()+72000,normal.begin()+72000),"Tank produces distinct stereo returns");
    check(normal==impulse(base),"Independent Prism instances repeat the same impulse");
    auto frozen=impulse(base,24000);
    check(frozen==impulse(base,24000,true) && energy({frozen.begin()+48000,frozen.begin()+72000})>0,
          "Freeze retains material and excludes later strikes from the tank");
    auto shifted=impulse({{"mix",1},{"predelay",10},{"echo",0},{"pitch",12},{"shift",1},{"motion",0},{"decay",.93f}});
    check(normal!=shifted,"Pitch shifting inside feedback changes the return");
    auto clocked=impulse({{"mix",1},{"sync",3},{"echo",1},{"feedback",0}});
    check(std::all_of(clocked.begin(),clocked.begin()+18000,[](float v){return v==0;}) &&
          std::abs(clocked[18000])>.25f,"Dotted eighth echo at exactly 18000 samples at 120 BPM");
    auto dry=impulse({{"mix",0}});
    check(dry[0]==.7f && energy({dry.begin()+1,dry.end()})<1e-12,"Dry stereo path preserved");
    impulse({{"size",1},{"decay",1.18f},{"pitch",-12},{"shift",1},{"rate",1},{"motion",1},
             {"feedback",.94f},{"tone",1},{"mix",1}});
}
