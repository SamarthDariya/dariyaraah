# Sourced by the experiment scripts. Not runnable on its own.
#
# The sweeps drive dariyanaap's CLI, which lives in the submodule and is NOT
# built by this repo's build: CMake adds vendor/dariyanaap as a subdirectory,
# and unit 0 deliberately suppresses its tests and executables when it is not
# the top-level project. That is the behaviour its vendor-smoke-test.sh exists
# to assert, and it is the right one — but it means the rig's CLI has to be
# built separately, once, and nothing said so until a fresh clone tried to run
# a sweep and got "no such file or directory".

RIG="vendor/dariyanaap/build/dariyanaap"
RIG_NULL="vendor/dariyanaap/build/dariyanaap-null"

ensure_rig() {
    if [[ -x "$RIG" && -x "$RIG_NULL" ]]; then
        return
    fi
    if [[ ! -f vendor/dariyanaap/CMakeLists.txt ]]; then
        echo "vendor/dariyanaap is empty. Run:" >&2
        echo "    git submodule update --init --recursive" >&2
        exit 1
    fi
    echo "--- building the rig's CLI (one-off) ---"
    cmake -S vendor/dariyanaap -B vendor/dariyanaap/build > /dev/null
    cmake --build vendor/dariyanaap/build --target dariyanaap dariyanaap-null -j > /dev/null
}
