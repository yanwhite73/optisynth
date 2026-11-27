// Offline stroke and gesture comparisons. The measured fallback requires no external WAV.
void handDrumChecks(){
    auto hit=[](const std::string& type,int key,float velocity,float radius,
                const std::vector<std::pair<std::string,float>>& edits,const SampleBank& bank=SampleBank{}){
        GraphSpec g;g.nodes={{"head",type,edits}};g.nodes[0].parameters.emplace_back("position",radius);
        g.outputs={{"head","out"}};Runtime r(g,bank);std::vector<float> out(48000);
        allocations=0;watchAllocations=true;
        r.render(out.data(),9600,{key,velocity,true,true});r.render(out.data()+9600,38400,{key,velocity,false,false});
        watchAllocations=false;check(allocations==0,"Hand drum render allocates nothing");
        return out;
    };
    auto difference=[](const std::vector<float>& a,const std::vector<float>& b){
        double sum=0,aa=0,bb=0;
        for(size_t i=0;i<a.size();++i){sum+=a[i]*b[i];aa+=a[i]*a[i];bb+=b[i]*b[i];}
        return 1-sum/std::sqrt(aa*bb);
    };
    for(auto model:{"drum.frame","drum.pakhawaj"}){
        const auto base=hit(model,48,.64f,.31f,{});
        check(energy(base)>1,"Head emits a measurable dry stroke");
        for(int key:{50,52,53,55})
            check(difference(base,hit(model,key,.64f,.31f,{}))>.05,"Distinct stroke types change timbre");
        check(difference(base,hit(model,48,.24f,.31f,{}))>.02,"Force changes spectrum independently of amplitude");
        check(difference(base,hit(model,48,.64f,.87f,{}))>.002,"Strike radius changes sound");
        check(difference(base,hit(model,48,.64f,.31f,{{"pressure",.9f}}))>.05,"Pressure acts on pitch and loss");
        auto noCoupling=hit(model,48,.64f,.31f,{{"couple",0}});
        check(noCoupling==hit(model,48,.64f,.31f,{{"couple",0},{"body_tune",7}}),"Passive head stays silent without energy transfer");
        check(difference(hit(model,48,.64f,.31f,{{"couple",1},{"body_tune",0}}),
                         hit(model,48,.64f,.31f,{{"couple",1},{"body_tune",7}}))>.00001,"Tuning an excited passive mode affects the response");
        // Request 146: a control that can carry movement must be audible across its range.
        const double sympathy=difference(noCoupling,hit(model,48,.64f,.31f,{{"couple",1}}));
        const double passiveTune=difference(hit(model,48,.64f,.31f,{{"couple",1},{"body_tune",-7}}),hit(model,48,.64f,.31f,{{"couple",1},{"body_tune",7}}));
        const double area=difference(hit(model,48,.64f,.31f,{{"area",0}}),hit(model,48,.64f,.31f,{{"area",1}}));
        check(sympathy>.1,"Sympathy is audible across its range");
        check(passiveTune>.1,"Passive tune is audible at full sympathy");
        check(area>.1,"Contact area is audible across its range");
    }
    // A tiny first-party test impulse occupies exactly one of thirty local frame layers.
    SampleBank bank;auto contact=std::make_shared<SampleData>();contact->mono.assign(4800,.3f);
    bank["head__skin_0"]=contact;
    auto local=[&](int key,float v,float pos,float mix,const SampleBank& b){
        return hit("drum.frame",key,v,pos,{{"skin",mix}},b);
    };
    check(local(48,.2f,0,1,bank)!=local(48,.2f,0,1,{}),"Matching type / zone / force selects its sample");
    for(auto args:{std::array<float,3>{50,.2f,0},std::array<float,3>{48,.95f,0},std::array<float,3>{48,.2f,1}})
        check(local(int(args[0]),args[1],args[2],1,bank)==local(int(args[0]),args[1],args[2],1,{}),
              "Other strikes, zones and velocity layers exclude that sample");
    check(local(48,.2f,0,0,bank)==local(48,.2f,0,0,{}),"Zero recorded skin gives the pure model");
}
