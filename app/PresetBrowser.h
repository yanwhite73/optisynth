#pragma once
#include "Panel.h"
#include "PresetLibrary.h"

// Request 71: preset browser overlay. Collections and categories on the left, search
// and tag chips above a sortable table, details / metadata editing on the right.
namespace {
const juce::String starOn=juce::String::fromUTF8("\xe2\x98\x85"),starOff=juce::String::fromUTF8("\xe2\x98\x86");
class PresetBrowser final : public juce::Component, private TableListBoxModel {
    // A small selectable list with counts (collections / categories).
    struct SideList final : ListBoxModel {
        struct Row { juce::String label;std::string value;int count=0; };
        std::vector<Row> rows;std::function<void(const std::string&)> chosen;bool refreshing=false;
        int getNumRows() override {return int(rows.size());}
        void paintListBoxItem(int row,Graphics& g,int width,int height,bool selected) override {
            if(row<0||row>=int(rows.size()))return;
            if(selected){g.setColour(teal.withAlpha(.28f));g.fillRoundedRectangle(2,1,float(width-4),float(height-2),4);}
            g.setColour(paper.withAlpha(selected?1.f:.82f));g.setFont(font(13));
            g.drawText(rows[size_t(row)].label,10,0,width-56,height,Justification::centredLeft);
            g.setColour(paper.withAlpha(.45f));g.setFont(font(11));g.drawText(String(rows[size_t(row)].count),width-48,0,38,height,Justification::centredRight);
        }
        void selectedRowsChanged(int row) override {if(!refreshing&&row>=0&&row<int(rows.size())&&chosen)chosen(rows[size_t(row)].value);}
    };
public:
    std::function<void(const PresetEntry&)> load;
    std::function<void()> close,importFile,exportFile;
    // Returns "" on success, "exists" when overwrite is needed, otherwise an error.
    std::function<std::string(const PresetInfo&,bool panelView,bool overwrite)> saveCurrent;
    std::function<void(const std::string& key,const PresetInfo&)> infoEdited;
    std::function<void()> favouritesChanged;
    PresetFilter filter;

    PresetBrowser(Preferences& preferences,juce::File libraryFolder):prefs(preferences),library(std::move(libraryFolder)){
        setWantsKeyboardFocus(true);
        for(auto* c:std::initializer_list<juce::Component*>{&collections,&categories,&search,&clearFilters,&table,&title,&meta,&description,&loadButton,&favouriteButton,
            &nameLabel,&nameEdit,&categoryLabel,&categoryBox,&tagsLabel,&tagsEdit,&notesLabel,&notesEdit,&panelToggle,&apply,&cancel,&note,&saveNew,
            &folderLabel,&addFolder,&reveal,&importButton,&exportButton,&keepSequence,&closeButton})addAndMakeVisible(c);
        for(auto* list:{&collections,&categories}){list->setRowHeight(28);list->setColour(ListBox::backgroundColourId,Colours::transparentBlack);list->setColour(ListBox::outlineColourId,Colours::transparentBlack);}
        collections.setModel(&collectionRows);categories.setModel(&categoryRows);
        collectionRows.chosen=[this](const std::string& v){filter.collection=v;applyFilter();};
        categoryRows.chosen=[this](const std::string& v){filter.category=v;applyFilter();};
        style(search);search.setTextToShowWhenEmpty("Search name, category, tag or description",paper.withAlpha(.4f));
        search.onTextChange=[this]{filter.search=search.getText().toStdString();applyFilter();};
        search.onEscapeKey=[this]{if(close)close();};
        clearFilters.onClick=[this]{filter=PresetFilter{};search.clear();applyFilter();};
        table.setModel(this);table.setRowHeight(26);table.setColour(ListBox::backgroundColourId,Colour(0xff1b1f22));
        table.setColour(ListBox::outlineColourId,Colours::transparentBlack);table.setOutlineThickness(0);table.getViewport()->setScrollBarsShown(true,false);
        auto& h=table.getHeader();
        h.addColumn(starOn,1,30,30,30,TableHeaderComponent::notResizableOrSortable);
        h.addColumn("Name",2,230,120,400,TableHeaderComponent::defaultFlags);
        h.addColumn("Updated",6,105,85,160,TableHeaderComponent::defaultFlags);
        h.addColumn("Category",3,100,70,160,TableHeaderComponent::defaultFlags);
        h.addColumn("Tags",4,190,80,600,TableHeaderComponent::defaultFlags);
        h.addColumn("Source",5,70,50,140,TableHeaderComponent::defaultFlags);
        h.setStretchToFitActive(true);h.setColour(TableHeaderComponent::backgroundColourId,Colour(0xff2a3034));h.setColour(TableHeaderComponent::textColourId,paper);
        h.setColour(TableHeaderComponent::outlineColourId,Colour(0xff1b1f22));
        title.setFont(font(21));title.setColour(Label::textColourId,paper);
        meta.setFont(font(12));meta.setColour(Label::textColourId,teal);
        description.setFont(font(12));description.setColour(Label::textColourId,paper.withAlpha(.8f));description.setJustificationType(Justification::topLeft);
        note.setFont(font(11));note.setColour(Label::textColourId,paper.withAlpha(.6f));note.setJustificationType(Justification::topLeft);
        for(auto* l:{&nameLabel,&categoryLabel,&tagsLabel,&notesLabel}){l->setFont(font(10));l->setColour(Label::textColourId,paper.withAlpha(.6f));}
        nameLabel.setText("NAME",dontSendNotification);categoryLabel.setText("CATEGORY",dontSendNotification);
        tagsLabel.setText("TAGS  (comma separated)",dontSendNotification);notesLabel.setText("DESCRIPTION",dontSendNotification);
        style(nameEdit);style(tagsEdit);style(notesEdit);notesEdit.setMultiLine(true,true);notesEdit.setReturnKeyStartsNewLine(true);
        categoryBox.setEditableText(true);for(size_t i=0;i<presetCategories().size();++i)categoryBox.addItem(presetCategories()[i],int(i)+1);
        categoryBox.setColour(ComboBox::backgroundColourId,Colour(0xff262b2f));categoryBox.setColour(ComboBox::textColourId,paper);categoryBox.setColour(ComboBox::outlineColourId,Colour(0xff3c4449));
        panelToggle.setButtonText("Open in synth panel view");panelToggle.setColour(ToggleButton::textColourId,paper);panelToggle.setColour(ToggleButton::tickColourId,paper);
        loadButton.onClick=[this]{if(const auto* e=selectedEntry())if(load)load(*e);};
        favouriteButton.onClick=[this]{if(const auto* e=selectedEntry())toggleFavourite(e->key);};
        apply.onClick=[this]{applyEdits(false);};cancel.onClick=[this]{saving=false;showDetails();};
        saveNew.onClick=[this]{if(requestSaveForm)requestSaveForm();};
        folderLabel.setFont(font(11));folderLabel.setColour(Label::textColourId,paper.withAlpha(.55f));
        addFolder.onClick=[this]{chooseFolder();};
        reveal.onClick=[this]{const auto* e=selectedEntry();(e&&!e->factory()?e->file:library).revealToUser();};
        importButton.onClick=[this]{if(importFile)importFile();};exportButton.onClick=[this]{if(exportFile)exportFile();};
        keepSequence.setButtonText("Keep current sequence");keepSequence.setColour(ToggleButton::textColourId,paper);keepSequence.setColour(ToggleButton::tickColourId,paper);
        keepSequence.setToggleState(prefs.flag("keepSequence",true),dontSendNotification);
        keepSequence.setTooltip("Presets with a sequencer load their own pattern and tempo. Other instruments can keep the current sequence.");
        keepSequence.onClick=[this]{prefs.set("keepSequence",keepSequence.getToggleState());};
        closeButton.onClick=[this]{if(close)close();};
        for(const auto& f:prefs.list("favourites"))favourites.insert(f.toStdString());
        setSortOrder(std::clamp(int(prefs.number("presetSortColumn",6)),0,6),prefs.flag("presetSortAscending",false));
        rescan();
    }
    ~PresetBrowser() override {table.setModel(nullptr);collections.setModel(nullptr);categories.setModel(nullptr);}
    std::function<void()> requestSaveForm;
    std::vector<juce::File> folders() const {std::vector<juce::File> f{library};for(const auto& extra:prefs.list("libraryFolders"))f.emplace_back(extra);return f;}
    const juce::File& libraryFolder() const {return library;}
    juce::File userFolder() const {return library.getChildFile("User");}   // request 73: user presets live in Presets/User
    void focusList(){
        table.grabKeyboardFocus();
        if(sortColumn==6 && !visibleRows.empty())table.scrollToEnsureRowIsOnscreen(0);
    }
    bool keepCurrentSequence() const {return keepSequence.getToggleState();}
    void setKeepSequence(bool keep){keepSequence.setToggleState(keep,dontSendNotification);prefs.set("keepSequence",keep);}
    void setSortOrder(int column,bool ascending){
        table.getHeader().setSortColumnId(column,ascending);
        sortOrderChanged(column,ascending);
    }
    void rescan(){
        problems.clear();entries=scanPresets(folders(),&problems);
        folderLabel.setText("Presets: "+library.getFullPathName()+(folders().size()>1?"  +"+String(int(folders().size())-1)+" folder(s)":"")+(problems.empty()?"":"  |  "+String(int(problems.size()))+" unreadable file(s)"),dontSendNotification);
        folderLabel.setTooltip(problems.empty()?String():String(joinLines(problems)));
        applyFilter();
    }
    void addFolderPath(const juce::File& folder){auto list=prefs.list("libraryFolders");list.addIfNotAlreadyThere(folder.getFullPathName());prefs.setList("libraryFolders",list);rescan();}
    void applyFilter(){
        // Sidebar counts: collections over everything, categories within the collection.
        auto count=[&](const PresetFilter& f){int n=0;for(const auto& e:entries)n+=presetMatches(e,f,favourites)?1:0;return n;};
        collectionRows.refreshing=categoryRows.refreshing=true;
        collectionRows.rows.clear();
        for(const auto& [label,value]:std::vector<std::pair<juce::String,std::string>>{{"All presets","all"},{starOn+"  Favourites","favourites"},{"Factory","factory"},{"User library","user"}}){
            PresetFilter f;f.collection=value;collectionRows.rows.push_back({label,value,count(f)});}
        categoryRows.rows.clear();PresetFilter within;within.collection=filter.collection;
        categoryRows.rows.push_back({"Any category","",count(within)});
        std::vector<std::string> names=presetCategories();
        for(const auto& e:entries)if(std::find(names.begin(),names.end(),e.category())==names.end())names.push_back(e.category());
        for(const auto& c:names){PresetFilter f=within;f.category=c;const int n=count(f);if(n>0||c==filter.category)categoryRows.rows.push_back({c,c,n});}
        collections.updateContent();categories.updateContent();
        for(size_t i=0;i<collectionRows.rows.size();++i)if(collectionRows.rows[i].value==filter.collection)collections.selectRow(int(i));
        for(size_t i=0;i<categoryRows.rows.size();++i)if(categoryRows.rows[i].value==filter.category)categories.selectRow(int(i));
        collectionRows.refreshing=categoryRows.refreshing=false;
        const auto keep=selectedKey.empty()?current:selectedKey;
        visibleRows.clear();for(size_t i=0;i<entries.size();++i)if(presetMatches(entries[i],filter,favourites))visibleRows.push_back(i);
        sortRows(visibleRows);rebuildChips();table.updateContent();table.repaint();
        selectKey(keep);showDetails();
    }
    size_t visibleCount() const {return visibleRows.size();}
    const PresetEntry& visibleEntry(size_t i) const {return entries[visibleRows[i]];}
    const PresetEntry* entry(const std::string& key) const {for(const auto& e:entries)if(e.key==key)return &e;return nullptr;}
    // Previous / next through the filtered, sorted list; wraps. When the current preset is
    // not in that list (or the list is empty) it steps through every preset instead.
    const PresetEntry* step(const std::string& from,int delta){
        std::vector<size_t> all(entries.size());for(size_t i=0;i<all.size();++i)all[i]=i;
        auto contains=[&](const std::vector<size_t>& rows){return std::any_of(rows.begin(),rows.end(),[&](size_t i){return entries[i].key==from;});};
        sortRows(all);
        const auto& rows=!visibleRows.empty()&&(contains(visibleRows)||!contains(all))?visibleRows:all;
        if(rows.empty())return nullptr;int at=-1;
        for(size_t i=0;i<rows.size();++i)if(entries[rows[i]].key==from)at=int(i);
        const int n=int(rows.size());const int next=at<0?(delta>0?0:n-1):((at+delta)%n+n)%n;
        const auto index=rows[size_t(next)];selectKey(entries[index].key);return &entries[index];
    }
    void setCurrent(const std::string& key){current=key;if(!saving)selectKey(key);table.repaint();}
    bool isFavourite(const std::string& key) const {return favourites.count(key)>0;}
    void toggleFavourite(const std::string& key){
        if(key.empty())return;if(!favourites.erase(key))favourites.insert(key);
        StringArray list;for(const auto& f:favourites)list.add(f);prefs.setList("favourites",list);
        applyFilter();if(favouritesChanged)favouritesChanged();
    }
    void showSaveForm(const PresetInfo& info,bool panelView){
        saving=true;title.setText("Save current sound",dontSendNotification);
        nameEdit.setText(info.name.empty()?"Untitled":info.name,false);setCategory(info.category.empty()?"Pad":info.category);
        tagsEdit.setText(joinTags(info.tags),false);notesEdit.setText(info.description,false);panelToggle.setToggleState(panelView,dontSendNotification);
        layoutDetails();nameEdit.grabKeyboardFocus();nameEdit.selectAll();
    }
    bool isSaving() const {return saving;}
    void selectKey(const std::string& key){
        int row=-1;for(size_t i=0;i<visibleRows.size();++i)if(entries[visibleRows[i]].key==key)row=int(i);
        selectedKey=row>=0?key:std::string{};
        if(row>=0){table.selectRow(row,false,true);table.scrollToEnsureRowIsOnscreen(row);}else table.deselectAllRows();
        if(!saving)showDetails();
    }
    // Tests: fill the save form and confirm overwrite without a dialog.
    std::string saveFromForm(bool overwrite){return applyEdits(overwrite);}
    void setFormFields(const std::string& name,const std::string& category,const std::string& tags,const std::string& notes){
        nameEdit.setText(name,false);setCategory(category);tagsEdit.setText(tags,false);notesEdit.setText(notes,false);
    }
    void toggleTag(const std::string& tag){if(!filter.tags.erase(tag))filter.tags.insert(tag);applyFilter();}
    // Arrow keys and Return always reach the list, wherever focus is in the browser (not while typing).
    bool keyPressed(const KeyPress& key) override {
        if(key==KeyPress::escapeKey){if(close)close();return true;}
        const bool listKey=key==KeyPress::upKey||key==KeyPress::downKey||key==KeyPress::pageUpKey||key==KeyPress::pageDownKey||key==KeyPress::returnKey;
        const bool typing=dynamic_cast<TextEditor*>(juce::Component::getCurrentlyFocusedComponent())!=nullptr;
        if(listKey&&!typing){
            if(!table.hasKeyboardFocus(true))table.grabKeyboardFocus();
            if(table.getSelectedRow()<0&&!visibleRows.empty()&&key!=KeyPress::returnKey){table.selectRow(0);return true;}
            return table.keyPressed(key);
        }
        return false;
    }
    void paint(Graphics& g) override {
        g.fillAll(Colour(0xff1f2427));
        g.setColour(Colour(0xff2b3236));g.drawVerticalLine(sideWidth,10,float(getHeight()-footer-6));g.drawVerticalLine(getWidth()-detailWidth-6,10,float(getHeight()-footer-6));
        g.drawHorizontalLine(getHeight()-footer-2,12,float(getWidth()-12));
        g.setColour(paper.withAlpha(.55f));g.setFont(font(10));
        g.drawText("COLLECTIONS",22,10,160,18,Justification::centredLeft);
        g.drawText("CATEGORIES",22,categories.getY()-22,160,18,Justification::centredLeft);
        g.drawText(String(int(visibleRows.size()))+" of "+String(int(entries.size()))+" presets",table.getX(),getHeight()-footer-24,260,18,Justification::centredLeft);
        if(tagChips.empty())g.drawText("No tags in these results",table.getX(),chipTop,300,26,Justification::centredLeft);
    }
    void resized() override {
        const int w=getWidth(),h=getHeight();detailWidth=std::clamp(w/4,300,360);
        collections.setBounds(12,30,sideWidth-24,4*28+4);
        categories.setBounds(12,30+4*28+40,sideWidth-24,h-footer-(30+4*28+40)-12);
        const int left=sideWidth+12,right=w-detailWidth-18;
        search.setBounds(left,10,right-left-110,30);clearFilters.setBounds(right-102,10,102,30);
        chipTop=48;layoutChips(left,right);
        table.setBounds(left,82,right-left,h-footer-82-28);
        const int y=h-footer+6;
        folderLabel.setBounds(16,y,std::max(120,w-16-(130+80+110+110+190+80)-12),28);
        int x=w-12;auto place=[&](juce::Component& c,int width){x-=width;c.setBounds(x,y,width,28);x-=8;};
        place(closeButton,80);place(keepSequence,182);place(exportButton,104);place(importButton,104);place(reveal,74);place(addFolder,110);
        layoutDetails();
    }
private:
    static constexpr int sideWidth=210,footer=46;
    int detailWidth=320,chipTop=48;
    Preferences& prefs;juce::File library;
    std::vector<PresetEntry> entries;std::vector<size_t> visibleRows;std::vector<std::string> problems;
    std::set<std::string> favourites;std::string selectedKey,current;bool saving=false;int sortColumn=0;bool sortForwards=true;
    SideList collectionRows,categoryRows;ListBox collections{"Collections"},categories{"Categories"};
    TextEditor search;TextButton clearFilters{"Clear filters"};TableListBox table{"Presets"};
    std::vector<std::unique_ptr<TextButton>> tagChips;
    Label title,meta,description,note,nameLabel,categoryLabel,tagsLabel,notesLabel,folderLabel;
    TextButton loadButton{"Load"},favouriteButton{starOff+" Favourite"},apply{"Save changes"},cancel{"Cancel"},saveNew{"Save current sound..."};
    TextEditor nameEdit,tagsEdit,notesEdit;ComboBox categoryBox;ToggleButton panelToggle;
    TextButton addFolder{"Add folder..."},reveal{"Reveal"},importButton{"Import file..."},exportButton{"Export file..."},closeButton{"Close"};ToggleButton keepSequence;
    std::unique_ptr<FileChooser> chooser;
    static String joinLines(const std::vector<std::string>& lines){String s;for(const auto& l:lines)s+=String(l)+"\n";return s.trimEnd();}
    static void style(TextEditor& e){e.setFont(font(13));e.setColour(TextEditor::backgroundColourId,Colour(0xff262b2f));e.setColour(TextEditor::textColourId,paper);
        e.setColour(TextEditor::outlineColourId,Colour(0xff3c4449));e.setColour(TextEditor::focusedOutlineColourId,teal);e.setIndents(8,6);}
    const PresetEntry* selectedEntry() const {return entry(selectedKey);}
    void setCategory(const std::string& c){categoryBox.setText(String(c),dontSendNotification);}
    void sortRows(std::vector<size_t>& rows) const {
        if(sortColumn==0)return;   // catalogue order: factory list, then user presets by name
        if(sortColumn==6){
            std::stable_sort(rows.begin(),rows.end(),[&](size_t a,size_t b){
                return sortForwards?entries[a].updated<entries[b].updated:entries[b].updated<entries[a].updated;
            });
            return;
        }
        auto key=[this](size_t i)->std::string{const auto& e=entries[i];
            if(sortColumn==3){const auto& c=presetCategories();const auto at=std::find(c.begin(),c.end(),e.category())-c.begin();return char('a'+at)+lowerCase(e.info.name);}
            if(sortColumn==4)return lowerCase(joinTags(e.info.tags));
            if(sortColumn==5)return std::string(e.factory()?"0":"1")+lowerCase(e.info.name);
            return lowerCase(e.info.name);};
        std::stable_sort(rows.begin(),rows.end(),[&](size_t a,size_t b){return sortForwards?key(a)<key(b):key(b)<key(a);});
    }
    void rebuildChips(){
        // Tags present in the results (ignoring the tag filter), most frequent first; selected tags always shown.
        PresetFilter loose=filter;loose.tags.clear();std::map<std::string,int> frequency;
        for(const auto& e:entries)if(presetMatches(e,loose,favourites))for(const auto& t:e.info.tags)++frequency[t];
        std::vector<std::pair<std::string,int>> ordered(frequency.begin(),frequency.end());
        std::stable_sort(ordered.begin(),ordered.end(),[&](const auto& a,const auto& b){
            const bool sa=filter.tags.count(a.first),sb=filter.tags.count(b.first);if(sa!=sb)return sa;return a.second>b.second;});
        tagChips.clear();
        for(const auto& [tag,n]:ordered){
            auto chip=std::make_unique<TextButton>(String(tag)+"  "+String(n));const bool on=filter.tags.count(tag)>0;
            chip->setColour(TextButton::buttonColourId,on?teal.withAlpha(.8f):Colour(0xff2a3135));chip->setColour(TextButton::textColourOffId,on?ink:paper.withAlpha(.85f));
            chip->setTooltip(on?"Remove this tag from the filter":"Show only presets with this tag");
            chip->onClick=[this,t=tag]{MessageManager::callAsync([safe=SafePointer<PresetBrowser>(this),t]{if(safe)safe->toggleTag(t);});};
            addAndMakeVisible(*chip);tagChips.push_back(std::move(chip));
        }
        for(const auto& t:filter.tags)if(!frequency.count(t)){
            auto chip=std::make_unique<TextButton>(String(t)+"  0");chip->setColour(TextButton::buttonColourId,teal.withAlpha(.8f));chip->setColour(TextButton::textColourOffId,ink);
            chip->onClick=[this,t]{MessageManager::callAsync([safe=SafePointer<PresetBrowser>(this),t]{if(safe)safe->toggleTag(t);});};addAndMakeVisible(*chip);tagChips.push_back(std::move(chip));}
        const int left=sideWidth+12,right=getWidth()-detailWidth-18;layoutChips(left,right);repaint();
    }
    void layoutChips(int left,int right){
        int x=left;Font f=font(12);
        for(auto& chip:tagChips){const int w=int(GlyphArrangement::getStringWidth(f,chip->getButtonText()))+22;
            if(x+w>right){chip->setVisible(false);continue;}chip->setVisible(true);chip->setBounds(x,chipTop,w,26);x+=w+6;}
    }
    void showDetails(){
        if(saving){layoutDetails();return;}
        const auto* e=selectedEntry();
        title.setText(e?String(e->info.name):"No preset selected",dontSendNotification);
        meta.setText(e?String(e->factory()?"Factory":"User")+"  /  "+String(e->category())+(e->view=="panel"?"  /  opens in panel view":""):String(),dontSendNotification);
        description.setText(e?String(e->info.description)+(e->factory()?"":"\n\n"+e->file.getFileName()):String("Choose a preset from the list. Double-click or press Return to load it."),dontSendNotification);
        favouriteButton.setButtonText(e&&isFavourite(e->key)?starOn+" Favourite":starOff+" Favourite");
        if(e&&!e->factory()){nameEdit.setText(e->info.name,false);setCategory(e->info.category);tagsEdit.setText(joinTags(e->info.tags),false);notesEdit.setText(e->info.description,false);}
        note.setText(e&&e->factory()?"Factory presets are read-only. Load one and use Save to keep an editable copy with your own tags.":"",dontSendNotification);
        layoutDetails();
    }
    void layoutDetails(){
        const int x=getWidth()-detailWidth,w=detailWidth-14,bottom=getHeight()-footer-8;const auto* e=selectedEntry();
        const bool editable=saving||(e&&!e->factory());
        title.setBounds(x,8,w,30);
        meta.setVisible(!saving);description.setVisible(!saving);loadButton.setVisible(!saving&&e);favouriteButton.setVisible(!saving&&e);
        int y=40;
        if(!saving){meta.setBounds(x,y,w,18);y+=20;description.setBounds(x,y,w,64);y+=68;
            loadButton.setBounds(x,y,90,30);favouriteButton.setBounds(x+98,y,118,30);y+=42;}
        for(auto* c:std::initializer_list<juce::Component*>{&nameLabel,&nameEdit,&categoryLabel,&categoryBox,&tagsLabel,&tagsEdit,&notesLabel,&notesEdit})c->setVisible(editable);
        panelToggle.setVisible(saving);cancel.setVisible(saving);apply.setVisible(editable);note.setVisible(!editable);
        apply.setButtonText(saving?"Save to library":"Save changes");
        if(editable){
            nameLabel.setBounds(x,y,w,16);nameEdit.setBounds(x,y+16,w,28);y+=50;
            categoryLabel.setBounds(x,y,w,16);categoryBox.setBounds(x,y+16,w,28);y+=50;
            tagsLabel.setBounds(x,y,w,16);tagsEdit.setBounds(x,y+16,w,28);y+=50;
            const int notesHeight=std::max(34,bottom-y-16-(saving?30:0)-44-38);
            notesLabel.setBounds(x,y,w,16);notesEdit.setBounds(x,y+16,w,notesHeight);y+=16+notesHeight+6;
            if(saving){panelToggle.setBounds(x,y,w,26);y+=30;}
            apply.setBounds(x,y,saving?140:130,30);if(saving)cancel.setBounds(x+148,y,90,30);
        } else note.setBounds(x,y,w,48);
        saveNew.setVisible(!saving);saveNew.setBounds(x,bottom-30,w,30);
    }
    std::string applyEdits(bool overwrite){
        PresetInfo info{nameEdit.getText().trim().toStdString(),categoryBox.getText().trim().substring(0,32).toStdString(),notesEdit.getText().substring(0,2000).toStdString(),parseTags(tagsEdit.getText().toStdString())};
        if(info.name.empty()){note.setText("A preset needs a name.",dontSendNotification);note.setVisible(true);return "A preset needs a name.";}
        info.name=info.name.substr(0,80);
        try{
            if(saving){
                if(!saveCurrent)return "No save handler";
                const auto result=saveCurrent(info,panelToggle.getToggleState(),overwrite);
                if(result=="exists"){
                    AlertWindow::showOkCancelBox(MessageBoxIconType::QuestionIcon,"Replace preset?","\""+String(info.name)+"\" already exists in the library. Replace it?","Replace","Cancel",nullptr,
                        ModalCallbackFunction::create([safe=SafePointer<PresetBrowser>(this)](int ok){if(safe&&ok)safe->applyEdits(true);}));
                    return result;
                }
                if(!result.empty())throw std::runtime_error(result);
                saving=false;rescan();return {};
            }
            const auto* e=selectedEntry();if(!e||e->factory())return "Factory presets are read-only";
            const auto key=e->key;updatePresetInfo(e->file,info);rescan();selectKey(key);if(infoEdited)infoEdited(key,info);return {};
        }catch(const std::exception& error){note.setText(error.what(),dontSendNotification);note.setVisible(true);return error.what();}
    }
    void chooseFolder(){
        chooser=std::make_unique<FileChooser>("Add a folder of .optisynth presets",File::getSpecialLocation(File::userDocumentsDirectory));
        chooser->launchAsync(FileBrowserComponent::openMode|FileBrowserComponent::canSelectDirectories,[safe=SafePointer<PresetBrowser>(this)](const FileChooser& c){
            if(safe&&c.getResult().isDirectory())safe->addFolderPath(c.getResult());});
    }
    // TableListBoxModel
    int getNumRows() override {return int(visibleRows.size());}
    void paintRowBackground(Graphics& g,int row,int width,int height,bool selected) override {
        if(row<0||row>=int(visibleRows.size()))return;
        g.fillAll(selected?teal.withAlpha(.3f):row%2?Colour(0xff1e2326):Colour(0xff1b1f22));
        if(entries[visibleRows[size_t(row)]].key==current){g.setColour(orange);g.fillRect(0,2,3,height-4);}
        juce::ignoreUnused(width);
    }
    void paintCell(Graphics& g,int row,int column,int width,int height,bool) override {
        if(row<0||row>=int(visibleRows.size()))return;const auto& e=entries[visibleRows[size_t(row)]];
        g.setFont(font(13));
        if(column==1){const bool on=isFavourite(e.key);g.setColour(on?orange:paper.withAlpha(.3f));g.drawText(on?starOn:starOff,0,0,width,height,Justification::centred);return;}
        g.setColour(column==2?paper:paper.withAlpha(.65f));if(column!=2)g.setFont(font(12));
        const String text=column==6?e.updated.formatted("%Y-%m-%d"):column==2?String(e.info.name):column==3?String(e.category()):column==4?String(joinTags(e.info.tags)):(e.factory()?String("Factory"):e.file.getParentDirectory().getFileName());
        g.drawText(text,6,0,width-10,height,Justification::centredLeft,true);
    }
    void cellClicked(int row,int column,const MouseEvent&) override {
        if(row>=0&&row<int(visibleRows.size())&&column==1){const auto key=entries[visibleRows[size_t(row)]].key;
            MessageManager::callAsync([safe=SafePointer<PresetBrowser>(this),key]{if(safe)safe->toggleFavourite(key);});}
    }
    void cellDoubleClicked(int row,int,const MouseEvent&) override {returnKeyPressed(row);}
    void returnKeyPressed(int row) override {if(row>=0&&row<int(visibleRows.size())&&load){const auto e=entries[visibleRows[size_t(row)]];load(e);}}
    void selectedRowsChanged(int row) override {
        if(row>=0&&row<int(visibleRows.size())){selectedKey=entries[visibleRows[size_t(row)]].key;if(saving)return;showDetails();}
    }
    void sortOrderChanged(int column,bool forwards) override {
        sortColumn=column;sortForwards=forwards;
        prefs.set("presetSortColumn",column);prefs.set("presetSortAscending",forwards);
        applyFilter();
        if(sortColumn==6 && !visibleRows.empty())table.scrollToEnsureRowIsOnscreen(0);
    }
};
}
