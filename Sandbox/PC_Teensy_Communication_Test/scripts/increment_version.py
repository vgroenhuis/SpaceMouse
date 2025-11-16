# type: ignore
import json
import os

def increment_version(env):
    path = os.path.join(env['PROJECT_DIR'], "version.json")

    with open(path, "r") as f:
        version = json.load(f)

    # Increment build number (or patch)
    version["build"] += 1

    # Write back to file
    with open(path, "w") as f:
        json.dump(version, f, indent=4)

    # Export to header file
    header = os.path.join(env['PROJECT_INCLUDE_DIR'], "version.h")
    with open(header, "w") as f:
        f.write(f'#define FW_MAJOR {version["major"]}\n')
        f.write(f'#define FW_MINOR {version["minor"]}\n')
        f.write(f'#define FW_PATCH {version["patch"]}\n')
        f.write(f'#define FW_BUILD {version["build"]}\n')
        f.write(f'#define FW_VERSION_STRING "{version["major"]}.{version["minor"]}.{version["patch"]}.{version["build"]}"\n')

    print("Generated version.h:", version)

def before_build(source, target, env):
    increment_version(env)

# Hook into PlatformIO
Import("env")  # type: ignore # noqa: F821
env.AddPreAction("$BUILD_DIR/src/main.cpp.o", before_build)
