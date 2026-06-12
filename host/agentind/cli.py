"""agentind CLI:状态源 → 协议帧 → 传输链路 的桥接主程序。

用法:
  agentind run                        # claude-code 源 + ws 自动发现
  agentind run -t can --channel can0  # 走 SocketCAN
  agentind run -t usb -s demo         # USB vendor EP + 演示动画
"""

from __future__ import annotations

import argparse
import asyncio
import logging

from . import sources, transports
from .protocol import Frame, MsgType, Telemetry

log = logging.getLogger("agentind")


async def bridge(source: sources.Source, transport: transports.Transport) -> None:
    def on_rx(frame: Frame) -> None:
        if frame.msg_type is MsgType.TELEMETRY:
            t = Telemetry.decode(frame.payload)
            log.info("telemetry: cells=%smV ibat=%dmA soc=%d%% src=%d",
                     t.cell_mv, t.ibat_ma, t.soc, t.power_src)
        elif frame.msg_type is MsgType.INPUT:
            log.info("device input: kind=%d arg=%d",
                     frame.payload[0], frame.payload[1])

    transport.on_rx = on_rx
    await transport.open()
    log.info("bridge up: %s", transport.name)
    try:
        async for frame in source.events():
            await transport.send(frame)
    finally:
        await transport.close()


def main() -> None:
    parser = argparse.ArgumentParser(prog="agentind")
    sub = parser.add_subparsers(dest="cmd", required=True)

    run = sub.add_parser("run", help="启动转发桥")
    run.add_argument("-t", "--transport", default="ws", choices=["ws", "can", "usb"])
    run.add_argument("-s", "--source", default="claude", choices=["claude", "demo"])
    run.add_argument("--url", help="ws 地址(缺省 mDNS 自动发现)")
    run.add_argument("--channel", default="can0", help="socketcan 通道名")
    run.add_argument("-v", "--verbose", action="store_true")

    args = parser.parse_args()
    logging.basicConfig(
        level=logging.DEBUG if args.verbose else logging.INFO,
        format="%(asctime)s %(name)s %(levelname)s %(message)s")

    tkwargs: dict = {}
    if args.transport == "ws" and args.url:
        tkwargs["url"] = args.url
    if args.transport == "can":
        tkwargs["channel"] = args.channel

    src = sources.create(args.source)
    tp = transports.create(args.transport, **tkwargs)
    try:
        asyncio.run(bridge(src, tp))
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
