"""Build the actual Keil source list with Arm Compiler 6, without opening uVision.
Usage: python Build/build_firmware.py --toolchain "D:/Program Files/Keil_v5/ARM/ARMCLANG/bin"
Outputs remain inside Build/arm. No download/programming is performed.
"""
import argparse
import pathlib
import subprocess
import sys
import xml.etree.ElementTree as ET

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.stdout.reconfigure(encoding="utf-8", errors="replace")
p = argparse.ArgumentParser()
p.add_argument("--toolchain", required=True)
args = p.parse_args()
tc = pathlib.Path(args.toolchain)
out = ROOT / "Build/arm"
out.mkdir(parents=True, exist_ok=True)
project = ET.parse(ROOT / "MDK-ARM/ChassisMotion.uvprojx")
paths = project.find(".//Cads/VariousControls/IncludePath").text.split(";")
includes = [str((ROOT / "MDK-ARM" / s.replace("\\", "/")).resolve()) for s in paths]
objects = []
log = []
def run(cmd):
    r = subprocess.run([str(c) for c in cmd], cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace")
    log.append(r.stdout)
    if r.stdout.strip(): print(r.stdout.strip())
    if r.returncode:
        (out / "build.log").write_text("\n".join(log), encoding="utf-8")
        raise SystemExit(r.returncode)
for f in project.findall(".//Groups/Group/Files/File"):
    src = (ROOT / "MDK-ARM" / f.findtext("FilePath").replace("\\", "/")).resolve()
    obj = out / (src.stem + ".o")
    objects.append(obj)
    if f.findtext("FileType") == "2":
        run([tc / "armasm.exe", "--cpu=Cortex-M3", "--apcs=interwork", "--pd", "__MICROLIB SETA 1", "-o", obj, src])
    else:
        run([tc / "armclang.exe", "--target=arm-arm-none-eabi", "-mcpu=cortex-m3", "-mthumb", "-std=c99", "-Oz", "-g",
             "-ffunction-sections", "-fdata-sections", "-fshort-enums", "-fshort-wchar", "-Wall", "-Wextra",
             "-D__MICROLIB", "-DUSE_HAL_DRIVER", "-DSTM32F103x6", *["-I" + s for s in includes], "-c", src, "-o", obj])
run([tc / "armlink.exe", "--cpu=Cortex-M3", "--library_type=microlib", "--strict", "--scatter", ROOT / "MDK-ARM/ChassisMotion.sct",
     "--map", "--symbols", "--info=sizes,totals,unused,stack", "--list", out / "ChassisMotion.map", "--output", out / "ChassisMotion.axf", *objects])
run([tc / "fromelf.exe", "--i32", "--output", out / "ChassisMotion.hex", out / "ChassisMotion.axf"])
(out / "build.log").write_text("\n".join(log), encoding="utf-8")
print("Built:", out / "ChassisMotion.axf")
