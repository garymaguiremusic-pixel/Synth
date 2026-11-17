# Compiles a vendored plugin's sources into a target, renaming the given identifiers with a prefix
# (per-file compile definitions, so the vendored files stay unmodified).
function(vc_add_vendor target dir prefix)
    set(names ${ARGN})
    set(defs "")
    foreach(n ${names})
        list(APPEND defs "${n}=${prefix}_${n}")
    endforeach()
    file(GLOB_RECURSE srcs CONFIGURE_DEPENDS ${dir}/*.cpp)
    target_sources(${target} PRIVATE ${srcs})
    set_source_files_properties(${srcs} TARGET_DIRECTORY ${target} PROPERTIES COMPILE_DEFINITIONS "${defs}")
endfunction()

set(VC_VD_RENAMES CustomLookAndFeel VirusColours VirusKnob KnobAttachment LcdDisplay LfoPreview ModeVisualizer
    TempoField HelpButton HelpOverlay createPluginFilter)
set(VC_LV_RENAMES CustomLookAndFeel VirusColours VirusKnob KnobAttachment LcdDisplay ReverbVisualizer
    TempoField HelpButton HelpOverlay createPluginFilter)
