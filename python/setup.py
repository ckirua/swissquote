from picobuild import Extension, cythonize, find_packages, get_cython_build_dir, setup

COMMON_C_FLAGS = [
    "-O3",
    "-march=native",
    "-Wno-unused-function",
    "-Wno-unused-variable",
]

decoder_extension = cythonize(
    [
        Extension(
            "swissquote.codec.bbo_decoder",
            ["src/swissquote/codec/bbo_decoder.pyx"],
            extra_compile_args=COMMON_C_FLAGS,
            language="c",
        ),
    ],
    build_dir=get_cython_build_dir(),
)

if __name__ == "__main__":
    setup(
        packages=find_packages(where="src"),
        package_dir={"": "src"},
        package_data={"swissquote": ["**/*.pxd", "**/*.pxi"]},
        ext_modules=decoder_extension,
    )
