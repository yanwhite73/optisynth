// Offline experiment: independently derived trapezoidal SVF with cubic band loss.
// Does not load a plugin or use decompiled code. Input is scalar reference data.
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>

constexpr double pi = 3.14159265358979323846, fs = 96000;
double f0 = 130.8127826503, cutoff = 523;
int sourceCount = 16;
constexpr int harmonics = 12, period = 734, samples = period * 32;
using Params = std::array<double, 5>;
struct Target { double osc, resonance; std::array<double, harmonics> db, weight; };

struct Model {
    double band = 0, low = 0, g, a, cubic;
    Model(double fc, double r, const Params& p) {
        g = std::tan(pi * fc * p[3] / fs);
        a = 1 + g * (g + p[0] * (1-r)*(1-r) + p[1]);
        cubic = p[2];
    }
    double tick(double x) {
        // Midpoint equation: a*b + cubic*b^3 = bandState + g*(x-lowState).
        // Its derivative is positive for every parameter admitted by the fitter.
        const double rhs = band + g * (x-low);
        double b = rhs / a;
        for (int i=0; i<4; ++i)
            b -= (b*(a+cubic*b*b)-rhs) / (a+3*cubic*b*b);
        band = 2*b-band;
        low += 2*g*b;
        return b;
    }
};

std::vector<double> inputWave, window;
std::array<std::vector<double>, harmonics> cosines, sines;
double windowSum = 0;

void prepare() {
    inputWave.resize(samples); window.resize(samples);
    double peak=1;
    for(int n=0;n<8192;++n) {
        double x=0; for(int h=1;h<=sourceCount;++h) x+=std::sin(2*pi*h*n/8192)/h;
        peak=std::max(peak,std::abs(x));
    }
    for(int h=0;h<harmonics;++h) { cosines[h].resize(samples); sines[h].resize(samples); }
    for(int n=0;n<samples;++n) {
        double x=0; for(int h=1;h<=sourceCount;++h) x+=std::sin(2*pi*f0*h*n/fs)/h;
        inputWave[n]=x/peak;
        // Warm up the first half, then measure steady harmonic amplitudes.
        window[n]=n<samples/2 ? 0 : .5-.5*std::cos(2*pi*(n-samples/2)/(samples/2-1));
        windowSum+=window[n];
        for(int h=0;h<harmonics;++h) {
            cosines[h][n]=window[n]*std::cos(2*pi*f0*(h+1)*n/fs);
            sines[h][n]=window[n]*std::sin(2*pi*f0*(h+1)*n/fs);
        }
    }
}

double evaluate(const Params& p,const std::vector<Target>& targets,bool report=false) {
    if(p[0]<.01 || p[0]>3 || p[1]<-.3 || p[1]>.8 || p[2]<.000001 || p[2]>4 ||
       p[3]<.7 || p[3]>1.3 || p[4]<.1 || p[4]>4) return 1e9;
    double error=0, weights=0;
    const double c=std::log2(cutoff/20)/10;
    for(const auto& t:targets) {
        Model model(cutoff,t.resonance,p);
        const double drive=std::pow(10,t.osc/20);
        const double mix=p[4]*.5*(1-(1-t.resonance)*(1-t.resonance))*(.6*c*c*c-4.4*c+4);
        std::array<double,harmonics> re{}, im{};
        for(int n=0;n<samples;++n) {
            const double x=inputWave[n]*drive;
            const double y=(x+mix*model.tick(x))*1.9275*std::pow(10.,-30./20);
            for(int h=0;h<harmonics;++h) { re[h]+=y*cosines[h][n]; im[h]+=y*sines[h][n]; }
        }
        if(report) std::cout << "case " << t.osc << ' ' << t.resonance;
        for(int h=0;h<harmonics;++h) {
            const double db=20*std::log10(std::max(1e-12,2*std::hypot(re[h],im[h])/windowSum));
            const double e=db-t.db[h]; error+=t.weight[h]*e*e; weights+=t.weight[h];
            if(report) std::cout << ' ' << db;
        }
        if(report) std::cout << '\n';
    }
    return std::sqrt(error/std::max(weights,1.));
}

int main(int argc,char** argv) {
    if(argc!=2 && argc!=5) return 2;
    if(argc==5) {cutoff=std::stod(argv[2]); f0=std::stod(argv[3]); sourceCount=std::stoi(argv[4]);}
    std::ifstream in(argv[1]); std::vector<Target> targets;
    Target t;
    while(in>>t.osc>>t.resonance) {
        for(int h=0;h<harmonics;++h) if(!(in>>t.db[h]>>t.weight[h])) return 3;
        targets.push_back(t);
    }
    if(targets.empty()) return 4;
    prepare();
    if(argc==5) {
        const auto score=evaluate({1.12969,-.11,.1825,.9825,1.96562},targets,true);
        std::cout << "held-out RMS dB error " << score << '\n';
        return std::isfinite(score) ? 0 : 5;
    }
    Params best{.6,-.05,.15,1.,1.};
    double score=evaluate(best,targets);
    std::array<double,5> steps{.15,.06,.08,.04,.15};
    for(int pass=0;pass<24;++pass) {
        bool improved=false;
        for(int j=0;j<5;++j) for(int dir:{-1,1}) {
            auto candidate=best; candidate[j]+=dir*steps[j];
            const double s=evaluate(candidate,targets);
            if(s<score) {score=s;best=candidate;improved=true;}
        }
        std::cout << "fit " << pass << ' ' << score;
        for(auto p:best) std::cout << ' ' << p;
        std::cout << std::endl;
        if(!improved) for(auto& step:steps) step*=.5;
    }
    evaluate(best,targets,true);
}
