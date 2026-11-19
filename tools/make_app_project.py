#!/usr/bin/env python3
"""Generate our native app definition using the established local JUCE modules."""
import pathlib, xml.etree.ElementTree as ET
root = pathlib.Path(__file__).resolve().parents[1]
base = ET.parse(root / 'plugin/Optisynth.jucer').getroot()
app = ET.Element('JUCERPROJECT', id='OptiApp2', name='Optisynth', projectType='guiapp',
    version='0.0.11', companyName='Yan White', bundleIdentifier='com.yanwhite.optisynth.app',
    useAppConfig='0', addUsingNamespaceToJuceHeader='0', cppLanguageStandard='17',
    jucerFormatVersion='1', defines='TEST=1', headerPath=str(root/'third_party/mutable'))
group = ET.SubElement(app, 'MAINGROUP', id='optiappsrc', name='Optisynth')
files = ['app/Main.cpp', 'src/modular/graph.cpp', 'src/modular/runtime.cpp',
         'src/modular/timing.cpp', 'src/modular/sequencer.cpp', 'src/modular/pattern.cpp', 'src/modular/performance.cpp', 'src/modular/analog.cpp',
         'src/core/spectrum.cpp', 'src/core/table_bank.cpp', 'src/core/patch.cpp', 'src/core/engine.cpp', 'src/core/arp.cpp', 'src/modular/spectral.cpp', 'src/modular/modules.cpp', 'src/modular/sample.cpp', 'src/core/envelope.cpp', 'src/core/effects.cpp',
         'third_party/mutable/plaits/dsp/physical_modelling/modal_voice.cc',
         'third_party/mutable/plaits/dsp/physical_modelling/resonator.cc',
         'third_party/mutable/plaits/resources_selected.cc',
         'third_party/mutable/stmlib/dsp/units.cc', 'third_party/mutable/stmlib/utils/random.cc']
for i, file in enumerate(files):
    ET.SubElement(group, 'FILE', id=f'optifile{i}', name=pathlib.Path(file).name,
                  compile='1', resource='0', file='../'+file)
modules = base.find('MODULES')
for m in list(modules):
    if m.attrib['id']=='juce_audio_plugin_client': modules.remove(m)
app.append(modules)
exporters=ET.SubElement(app,'EXPORTFORMATS')
exporter=ET.SubElement(exporters,'XCODE_MAC', targetFolder='Builds/MacOSX', extraFrameworks='ApplicationServices')
configs=ET.SubElement(exporter,'CONFIGURATIONS')
ET.SubElement(configs,'CONFIGURATION',name='Release',isDebug='0',optimisation='3',targetName='Optisynth')
paths=ET.SubElement(exporter,'MODULEPATHS')
for m in modules:ET.SubElement(paths,'MODULEPATH',id=m.attrib['id'],path='/Users/yanwhite/JUCE/modules')
ET.SubElement(app,'JUCEOPTIONS',JUCE_STRICT_REFCOUNTEDPOINTER='1',JUCE_WEB_BROWSER='0',JUCE_USE_CURL='0')
ET.indent(app)
ET.ElementTree(app).write(root/'app/OptisynthApp.jucer',encoding='UTF-8',xml_declaration=True)
