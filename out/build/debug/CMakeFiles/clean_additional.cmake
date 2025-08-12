# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles\\SystemAudioSpectrum_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\SystemAudioSpectrum_autogen.dir\\ParseCache.txt"
  "SystemAudioSpectrum_autogen"
  )
endif()
