# Exercise the published model before packaging on each desktop platform.
if(NOT DEFINED MODEL_FILE)
    message(FATAL_ERROR "Set MODEL_FILE to the local path for the test model.")
endif()
file(DOWNLOAD
    "https://huggingface.co/Ruicheng/moge-2-vits-normal-onnx/resolve/e50ffda41565591092adea54c6ac83d6212e1e23/model.onnx"
    "${MODEL_FILE}"
    EXPECTED_HASH SHA256=24eacb5dc7a2c54c7bc98f7de085ffbed79ad006ea5b664c2c2cdc02ff3a52f0
    TLS_VERIFY TRUE
    TIMEOUT 240
)
