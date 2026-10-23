# Source this before idf.py:   . ./env.sh
#
# Pins ESP-IDF's Python env to the 3.12 one. The py3.14 env on this machine
# fails export.sh's requirements check (esp-coredump 1.17.0 is excluded by the
# v5.3 constraints file). Also drops any active venv, which IDF refuses to run in.
deactivate 2>/dev/null; unset VIRTUAL_ENV
export IDF_PYTHON_ENV_PATH="$HOME/.espressif/python_env/idf5.3_py3.12_env"
. "$HOME/esp/esp-idf/export.sh" >/dev/null
