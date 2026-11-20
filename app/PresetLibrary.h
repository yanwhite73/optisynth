#pragma once
#include <JuceHeader.h>
#include "PatchIO.h"
#include <set>
#include <sstream>

// Request 71: preset catalogue (factory starters + user library folders), favourites
// and filtering. Category, tags and description live in each patch file; favourites,
// zoom, view preference and extra folders are per-user settings.
namespace optisynth {
class Preferences {
public:
    explicit Preferences(const juce::File& file):properties(file,options()){}
    juce::String text(const juce::String& key,const juce::String& fallback={}) const {return properties.getValue(key,fallback);}
    double number(const juce::String& key,double fallback) const {return properties.getDoubleValue(key,fallback);}
    bool flag(const juce::String& key,bool fallback) const {return properties.getBoolValue(key,fallback);}
    juce::StringArray list(const juce::String& key) const {juce::StringArray a;a.addLines(properties.getValue(key));a.removeEmptyStrings();return a;}
    template<typename T> void set(const juce::String& key,const T& value){properties.setValue(key,juce::var(value));}
    void setList(const juce::String& key,const juce::StringArray& values){properties.setValue(key,values.joinIntoString("\n"));}
    juce::File file() const {return properties.getFile();}
private:
    static juce::PropertiesFile::Options options(){juce::PropertiesFile::Options o;o.millisecondsBeforeSaving=0;o.storageFormat=juce::PropertiesFile::storeAsXML;return o;}
    mutable juce::PropertiesFile properties;
};
struct PresetEntry {
    std::string key;PresetInfo info;std::string view;int starter=-1;juce::File file;
    bool factory() const {return starter>=0;}
    std::string category() const {return info.category.empty()?"Uncategorised":info.category;}
};
inline std::string lowerCase(std::string s){for(auto& c:s)c=char(std::tolower(static_cast<unsigned char>(c)));return s;}
inline std::string joinTags(const std::vector<std::string>& tags){std::string s;for(const auto& t:tags)s+=(s.empty()?"":", ")+t;return s;}
// Reads only the browser fields of a patch file; full validation happens on load.
inline PresetEntry readPresetEntry(const juce::File& file) {
    if(file.getSize()>2*1024*1024)throw std::runtime_error("Patch file too large");
    const auto value=juce::JSON::parse(file);
    if(!value.isObject()||(value["format"].toString()!="optisynth.rack"&&value["format"].toString()!="optisynth.instrument"))throw std::runtime_error("Not an Optisynth patch");
    PresetEntry e{"user:"+file.getFullPathName().toStdString(),{},{},-1,file};
    if(value["version"].isInt()&&int(value["version"])>=6){RackDocument d;readPresetFields(value,d);e.info=d.info;e.view=d.view;}
    if(e.info.name.empty())e.info.name=file.getFileNameWithoutExtension().toStdString();
    return e;
}
inline std::vector<PresetEntry> scanPresets(const std::vector<juce::File>& folders,std::vector<std::string>* problems=nullptr) {
    std::vector<PresetEntry> list;
    for(const auto& f:factoryPresets())list.push_back({"factory:"+std::to_string(f.starter),f.info,f.view,f.starter,{}});
    std::set<std::string> seen;std::vector<PresetEntry> user;
    for(const auto& folder:folders){
        if(!folder.isDirectory())continue;
        auto files=folder.findChildFiles(juce::File::findFiles,true,"*.optisynth");files.sort();
        for(const auto& file:files){
            if(user.size()>=2000)break;
            if(!seen.insert(file.getFullPathName().toStdString()).second)continue;
            try{user.push_back(readPresetEntry(file));}
            catch(const std::exception& e){if(problems)problems->push_back(file.getFileName().toStdString()+": "+e.what());}
        }
    }
    std::stable_sort(user.begin(),user.end(),[](const PresetEntry& a,const PresetEntry& b){return lowerCase(a.info.name)<lowerCase(b.info.name);});
    list.insert(list.end(),user.begin(),user.end());
    return list;
}
struct PresetFilter {
    std::string collection="all";   // all, favourites, factory, user
    std::string category;           // empty: every category
    std::set<std::string> tags;     // all must match
    std::string search;             // every word must appear
};
inline bool presetMatches(const PresetEntry& e,const PresetFilter& f,const std::set<std::string>& favourites) {
    if(f.collection=="favourites"&&!favourites.count(e.key))return false;
    if(f.collection=="factory"&&!e.factory())return false;
    if(f.collection=="user"&&e.factory())return false;
    if(!f.category.empty()&&e.category()!=f.category)return false;
    for(const auto& t:f.tags)if(std::find(e.info.tags.begin(),e.info.tags.end(),t)==e.info.tags.end())return false;
    if(!f.search.empty()){
        const auto haystack=lowerCase(e.info.name+" "+e.category()+" "+joinTags(e.info.tags)+" "+e.info.description);
        std::istringstream words(lowerCase(f.search));std::string word;
        while(words>>word)if(haystack.find(word)==std::string::npos)return false;
    }
    return true;
}
inline juce::File presetFileFor(const juce::File& folder,const std::string& name) {
    auto base=juce::File::createLegalFileName(juce::String(name).trim());if(base.isEmpty())base="Untitled";
    return folder.getChildFile(base+".optisynth");
}
// Writes through a temporary sibling so a failed write leaves the old file intact.
inline void writePresetFile(const juce::File& file,const juce::var& json) {
    readRack(json);   // never write a file this app cannot open again
    file.getParentDirectory().createDirectory();
    juce::TemporaryFile temporary(file);
    if(!temporary.getFile().replaceWithText(juce::JSON::toString(json))||!temporary.overwriteTargetFileWithTemporary())
        throw std::runtime_error("Could not write "+file.getFullPathName().toStdString());
}
// Changes only the browser fields of a user preset; everything else is kept as written.
inline void updatePresetInfo(const juce::File& file,const PresetInfo& info) {
    if(file.getSize()>2*1024*1024)throw std::runtime_error("Patch file too large");
    auto value=juce::JSON::parse(file);
    if(!value.isObject()||value["format"].toString()!="optisynth.rack"||!value["version"].isInt()||int(value["version"])<6){
        auto d=readRack(value);d.info=info;writePresetFile(file,rackJSON(d));return;   // older formats are upgraded whole
    }
    auto* object=value.getDynamicObject();auto* fields=new juce::DynamicObject;
    fields->setProperty("name",juce::String(info.name));fields->setProperty("category",juce::String(info.category));
    fields->setProperty("description",juce::String(info.description));
    juce::Array<juce::var> tags;for(const auto& t:info.tags)tags.add(juce::String(t));fields->setProperty("tags",tags);
    object->setProperty("info",juce::var(fields));
    writePresetFile(file,value);
}
}
