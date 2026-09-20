function(vellum_verify_dependency name path expected_sha256)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "${name} is missing: ${path}")
    endif()
    file(SHA256 "${path}" actual_sha256)
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(FATAL_ERROR
            "${name} failed SHA-256 verification.\n"
            "Expected: ${expected_sha256}\n"
            "Actual:   ${actual_sha256}\n"
            "Path:     ${path}")
    endif()
endfunction()

vellum_verify_dependency("Qt 6 Core" "${VELLUM_QT_BIN}/Qt6Core.dll"
    "b712b4754588e89f855dc0aa087d6304b2fd21b0e0639d1cdeb9cb969bd5fb72")
vellum_verify_dependency("Qt 6 GUI" "${VELLUM_QT_BIN}/Qt6Gui.dll"
    "03ba47a2025036a04f5548bce242788ac21edf551e78d5b42e1b87beb799904f")
vellum_verify_dependency("Qt 6 Widgets" "${VELLUM_QT_BIN}/Qt6Widgets.dll"
    "61ddd0316a731b8dbabaf2924cd0d47b756461e37329b4f535902cf450416127")
vellum_verify_dependency("Qt Windows platform plugin" "${VELLUM_QT_PLUGINS}/platforms/qwindows.dll"
    "cafd5bfdef3d89d176ee26725c8d4759fd6cb007645b645a4cbf6386364a3f36")
vellum_verify_dependency("Qt offscreen test plugin" "${VELLUM_QT_PLUGINS}/platforms/qoffscreen.dll"
    "a35f87f597b91121cfc0ef39dcb0a5e7ad2876850fbfaf6377edf80eaa067bfd")
vellum_verify_dependency("libsodium runtime" "${SODIUM_ROOT}/x64/Release/v143/dynamic/libsodium.dll"
    "f656aeb789bfc3a2ac587fae1d7cfe278bbe8ce73f28da73b4d08282ea8f9fe3")
vellum_verify_dependency("libsodium import library" "${SODIUM_ROOT}/x64/Release/v143/dynamic/libsodium.lib"
    "02157c05a1f0ebd4e99521da1363ad48eb17e47edfdf90fb10febba9960593fe")
vellum_verify_dependency("libsodium public header" "${SODIUM_ROOT}/include/sodium.h"
    "29d5d70a432320fb40bc1234302f2bf59ba2e78c5e61494b894d35b18e0feacf")
