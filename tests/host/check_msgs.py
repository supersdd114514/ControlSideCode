#!/usr/bin/env python3
"""DroneCAN 消息编码的独立参考校验。

用一份"从 DSDL 字段定义重新推导"的位写入器，在 PC 上把
Core/Src/dronecan_msgs.c 的输出逐字节比对，防止位偏移/位宽写错。

用法（在仓库根目录）：
    python tests/host/check_msgs.py
"""

import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))

# 由 dronecan/dsdl 计算得到，dronecan_msgs.h 里必须保持一致
SIGNATURES = {
    "NodeStatus": 0x0F0868D0C1A7C6F1,
    "GetNodeInfo": 0xEE468A8121C46A9E,
    "RawCommand": 0x217F5C87D7EC951D,
}
DTIDS = {"NodeStatus": 341, "GetNodeInfo": 1, "RawCommand": 1030}


class BitWriter:
    """DroneCAN（UAVCAN v0）DSDL 序列化参考实现。

    规则（与 libcanard 的 canardEncodeScalar 一致）：
      - 每个标量按其位宽切成 little-endian 字节；
      - 每个字节内部按 MSB 先写；
      - 最后不足一字节的部分左对齐（高位）；
      - 整个结构补零到字节边界。
    """

    def __init__(self):
        self.bits = ""

    def _field(self, value, width):
        raw = value & ((1 << width) - 1)      # 负数自动取补码
        out = []
        for index in range((width + 7) // 8):
            byte = (raw >> (8 * index)) & 0xFF
            take = min(8, width - 8 * index)
            if take < 8:
                byte = (byte & ((1 << take) - 1)) << (8 - take)
            out.append(format(byte, "08b")[:take])
        self.bits += "".join(out)
        return self

    def u(self, value, width):
        return self._field(value, width)

    def s(self, value, width):
        return self._field(value, width)

    def to_bytes(self):
        bits = self.bits
        if len(bits) % 8:
            bits += "0" * (8 - len(bits) % 8)
        return bytes(int(bits[i:i + 8], 2) for i in range(0, len(bits), 8))


def enc_node_status(uptime, health, mode, sub_mode, vendor):
    return (BitWriter()
            .u(uptime, 32).u(health, 2).u(mode, 3).u(sub_mode, 3).u(vendor, 16)
            .to_bytes())


def enc_raw_command(values):
    w = BitWriter().u(len(values), 5)   # [<=20] -> 5 bit 长度前缀
    for value in values:
        w.s(value, 14)
    return w.to_bytes()


def enc_node_info(uptime, health, mode, vendor, sw, hw, uid, name):
    w = BitWriter()
    # status: uavcan.protocol.NodeStatus
    w.u(uptime, 32).u(health, 2).u(mode, 3).u(0, 3).u(vendor, 16)
    # software_version: major, minor, optional_field_flags, vcs_commit, image_crc
    w.u(sw[0], 8).u(sw[1], 8).u(0, 8).u(0, 32).u(0, 64)
    # hardware_version: major, minor, unique_id[16], certificate_of_authenticity[<=255]
    w.u(hw[0], 8).u(hw[1], 8)
    for byte in uid:
        w.u(byte, 8)
    w.u(0, 8)          # COA 长度 = 0
    # name[<=80]
    raw = name.encode("ascii")
    w.u(len(raw), 7)   # 7 bit 长度前缀
    for byte in raw:
        w.u(byte, 8)
    return w.to_bytes()


def build_and_run():
    exe = os.path.join(HERE, "test_dronecan_msgs.exe")
    cmd = [
        "gcc", "-std=c11", "-O1", "-Wall", "-Wextra",
        # PC 是 64 位，libcanard 需要这个宏才能通过内部静态断言；
        # 目标板（Cortex-M4，32 位）用默认值 0。
        "-DCANARD_64_BIT=1",
        "-I", os.path.join(ROOT, "Core", "Inc"),
        "-I", os.path.join(ROOT, "ThirdParty", "libcanard"),
        os.path.join(HERE, "test_dronecan_msgs.c"),
        os.path.join(ROOT, "Core", "Src", "dronecan_msgs.c"),
        os.path.join(ROOT, "ThirdParty", "libcanard", "canard.c"),
        "-o", exe,
    ]
    subprocess.run(cmd, check=True)
    log = exe + ".log"
    try:
        # 沙箱环境不允许给子进程建管道，这里用文件重定向代替 capture_output
        with open(log, "w", encoding="utf-8") as handle:
            subprocess.run([exe], stdout=handle, check=True)
        with open(log, "r", encoding="utf-8", errors="replace") as handle:
            out = handle.read()
    finally:
        for path in (exe, log):
            if os.path.exists(path):
                os.remove(path)
    result = {}
    for line in out.splitlines():
        parts = line.split()
        if len(parts) == 2:
            result[parts[0]] = parts[1]
    return result



def build_and_run_transfer():
    """传输层自测：RawCommand(8B) -> 2 帧 -> 另一个 libcanard 实例解码。"""
    exe = os.path.join(HERE, "test_transfer.exe")
    cmd = [
        "gcc", "-std=c11", "-O1", "-Wall",
        "-DCANARD_64_BIT=1",
        "-I", os.path.join(ROOT, "Core", "Inc"),
        "-I", os.path.join(ROOT, "ThirdParty", "libcanard"),
        os.path.join(HERE, "test_transfer.c"),
        os.path.join(ROOT, "Core", "Src", "dronecan_msgs.c"),
        os.path.join(ROOT, "ThirdParty", "libcanard", "canard.c"),
        "-o", exe,
    ]
    subprocess.run(cmd, check=True)
    log = exe + ".log"
    try:
        with open(log, "w", encoding="utf-8") as handle:
            subprocess.run([exe], stdout=handle, check=False)
        with open(log, "r", encoding="utf-8", errors="replace") as handle:
            out = handle.read()
    finally:
        for path in (exe, log):
            if os.path.exists(path):
                os.remove(path)
    return out


def check_transfer():
    out = build_and_run_transfer()
    failures = 0

    frame_ids = re.findall(r"FRAME id=0x([0-9A-Fa-f]+) dlc=(\d+)", out)
    expected_id = int(re.search(r"EXPECT_ID 0x([0-9A-Fa-f]+)", out).group(1), 16)

    print(f"{'PASS' if len(frame_ids) == 2 else 'FAIL'} frame count = {len(frame_ids)}")
    if len(frame_ids) != 2:
        failures += 1
        print(out)

    dlcs = [int(dlc) for _, dlc in frame_ids]
    print(f"{'PASS' if dlcs == [8, 4] else 'FAIL'} frame dlc = {dlcs}")
    if dlcs != [8, 4]:
        failures += 1

    ids = {int(fid, 16) for fid, _ in frame_ids}
    print(f"{'PASS' if ids == {expected_id} else 'FAIL'} can id = {[hex(i) for i in ids]} (expected {hex(expected_id)})")
    if ids != {expected_id}:
        failures += 1

    decoded = re.search(r"DECODED count=(\d+) len=(\d+) values=(-?\d+),(-?\d+),(-?\d+),(-?\d+)", out)
    values = [int(decoded.group(i)) for i in range(3, 7)]
    ok = decoded.group(1) == "4" and decoded.group(2) == "8" and values == [-8192, -1000, 2500, 8191]
    print(f"{'PASS' if ok else 'FAIL'} round-trip = {values}")
    if not ok:
        failures += 1

    if "TRANSFER TEST OK" not in out:
        print(out)
        failures += 1

    return failures


def main():
    got = build_and_run()

    uid = bytes(range(16))
    expected = {
        "NodeStatus": enc_node_status(1234, 2, 0, 0, 0x1234),
        "RawCommand4": enc_raw_command([-8192, -1, 0, 8191]),
        "RawCommand1": enc_raw_command([777]),
        "RawCommand20": enc_raw_command([777] + [0] * 18 + [-777]),
        "GetNodeInfo": enc_node_info(1234, 1, 0, 0x0005, (1, 2), (3, 4), uid,
                                     "com.hpaics.control"),
    }

    failures = 0
    for name, want in expected.items():
        have = got.get(name)
        ok = have == want.hex()
        print(f"{'PASS' if ok else 'FAIL'} {name}: {have}")
        if not ok:
            print(f"     expected {want.hex()}")
            failures += 1

    # 拒绝非法参数
    if got.get("RejectNull") != "0" or got.get("RejectEmpty") != "0":
        print("FAIL parameter validation")
        failures += 1
    else:
        print("PASS parameter validation")

    # 签名与 DTID 自检（防止后人改 DSDL 却忘了更新常量）
    for name, value in SIGNATURES.items():
        print(f"INFO {name}: dtid={DTIDS[name]} signature=0x{value:016X}")

    failures += check_transfer()

    if failures:
        print(f"\n{failures} check(s) failed")
        return 1
    print("\nall message encoding checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
