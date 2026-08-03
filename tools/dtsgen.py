#!/usr/bin/env python3
"""Compile the HalaOS educational DTS subset to a table-driven compact DTB v2."""
from __future__ import annotations
import argparse, json, re, struct, zlib
from pathlib import Path

DTB_MAGIC = 0x48445442
DTB_VERSION = 2
DT_STRING, DT_U32, DT_U32_PAIR, DT_U32_TRIPLE, DT_BOOL = 1, 2, 3, 4, 5

def parse_num(text: str) -> int: return int(text, 0)
def pstr(s: str) -> bytes: return s.encode("utf-8") + b"\0"
def pu32(v: int) -> bytes: return struct.pack("<I", v)
def prop(name: str, typ: int, value: bytes) -> bytes:
    nb=pstr(name)
    return struct.pack("<BBH",len(nb),typ,len(value))+nb+value

def node(path: str, props: list[bytes]) -> bytes:
    pb=pstr(path); body=pb+b"".join(props)
    return struct.pack("<BBH",len(pb),len(props),4+len(body))+body

def main() -> int:
    ap=argparse.ArgumentParser()
    ap.add_argument('dts',type=Path);ap.add_argument('--out-h',type=Path,required=True);ap.add_argument('--out-json',type=Path,required=True)
    ap.add_argument('--out-dtb',type=Path);ap.add_argument('--out-dtb-map',type=Path);ap.add_argument('--out-c',type=Path)
    a=ap.parse_args(); text=a.dts.read_text(encoding='utf-8')
    model=re.search(r'\bmodel\s*=\s*"([^"]+)"',text)
    compat=re.search(r'/\s*\{.*?\bcompatible\s*=\s*"([^"]+)"',text,re.S)
    init=re.search(r'hala,init\s*=\s*"([^"]+)"',text)
    mem=re.search(r'memory@([0-9a-fA-F]+)\s*\{[^}]*reg\s*=\s*<\s*(0x[0-9a-fA-F]+)\s+(0x[0-9a-fA-F]+)',text,re.S)
    usart=re.search(r'usart1:\s*serial@([0-9a-fA-F]+)\s*\{(.*?)\};',text,re.S)
    led=re.search(r'status_led:\s*led\s*\{(.*?)\};',text,re.S)
    ga=re.search(r'gpioa:\s*gpio@([0-9a-fA-F]+)\s*\{(.*?)\};',text,re.S)
    gc=re.search(r'gpioc:\s*gpio@([0-9a-fA-F]+)\s*\{(.*?)\};',text,re.S)
    if not all([model,compat,init,mem,usart,led,ga,gc]): raise SystemExit('DTS missing required nodes')
    ub,lb=usart.group(2),led.group(1)
    baud=re.search(r'(?:baudrate|current-speed)\s*=\s*<(\d+)>',ub);irq=re.search(r'interrupts\s*=\s*<(\d+)>',ub)
    tx=re.search(r'tx-pin\s*=\s*<&gpio([a-z])\s+(\d+)\s+(\d+)>',ub);rx=re.search(r'rx-pin\s*=\s*<&gpio([a-z])\s+(\d+)\s+(\d+)>',ub)
    lp=re.search(r'(?:pin|gpios)\s*=\s*<&gpio([a-z])\s+(\d+)\s+(\d+)>',lb)
    if not all([baud,irq,tx,rx,lp]): raise SystemExit('DTS pin/baud/irq incomplete')
    pi=lambda p:ord(p.upper())-ord('A')
    cfg={
      'model':model.group(1),'compatible':compat.group(1),'init':init.group(1),
      'memoryBase':parse_num(mem.group(2)),'memorySize':parse_num(mem.group(3)),
      'usart1Base':int(usart.group(1),16),'usart1Irq':int(irq.group(1)),'baudrate':int(baud.group(1)),
      'tx':{'port':tx.group(1).upper(),'pin':int(tx.group(2)),'mode':int(tx.group(3))},
      'rx':{'port':rx.group(1).upper(),'pin':int(rx.group(2)),'mode':int(rx.group(3))},
      'led':{'port':lp.group(1).upper(),'pin':int(lp.group(2)),'mode':int(lp.group(3)),'activeLow':'active-low' in lb},
    }
    if cfg['memoryBase']!=0x20000000 or cfg['memorySize']>0x5000: raise SystemExit('RAM contract violated')
    stdout=f"/soc/usart1"
    nodes=[
      node('/',[prop('model',DT_STRING,pstr(cfg['model'])),prop('compatible',DT_STRING,pstr(cfg['compatible']))]),
      node('/memory',[prop('device_type',DT_STRING,pstr('memory')),prop('reg',DT_U32_PAIR,pu32(cfg['memoryBase'])+pu32(cfg['memorySize']))]),
      node('/chosen',[prop('stdout-path',DT_STRING,pstr(stdout)),prop('hala,init',DT_STRING,pstr(cfg['init'])),prop('status-led',DT_STRING,pstr('/leds/status'))]),
      node('/soc/gpioa',[prop('compatible',DT_STRING,pstr('st,stm32f1-gpio')),prop('reg',DT_U32_PAIR,pu32(int(ga.group(1),16))+pu32(0x400)),prop('status',DT_STRING,pstr('okay'))]),
      node('/soc/gpioc',[prop('compatible',DT_STRING,pstr('st,stm32f1-gpio')),prop('reg',DT_U32_PAIR,pu32(int(gc.group(1),16))+pu32(0x400)),prop('status',DT_STRING,pstr('okay'))]),
      node('/soc/usart1',[prop('compatible',DT_STRING,pstr('st,stm32f1-usart')),prop('reg',DT_U32_PAIR,pu32(cfg['usart1Base'])+pu32(0x400)),prop('current-speed',DT_U32,pu32(cfg['baudrate'])),prop('interrupts',DT_U32,pu32(cfg['usart1Irq'])),prop('tx-pin',DT_U32_TRIPLE,pu32(pi(cfg['tx']['port']))+pu32(cfg['tx']['pin'])+pu32(cfg['tx']['mode'])),prop('rx-pin',DT_U32_TRIPLE,pu32(pi(cfg['rx']['port']))+pu32(cfg['rx']['pin'])+pu32(cfg['rx']['mode'])),prop('status',DT_STRING,pstr('okay'))]),
      node('/leds/status',[prop('compatible',DT_STRING,pstr('hala,gpio-output')),prop('pin',DT_U32_TRIPLE,pu32(pi(cfg['led']['port']))+pu32(cfg['led']['pin'])+pu32(cfg['led']['mode'])),prop('active-low',DT_BOOL,b'\1' if cfg['led']['activeLow'] else b'\0')]),
    ]
    prop_count=2+2+3+3+3+7+3
    records=b''.join(nodes); header=struct.pack('<IIIIII',DTB_MAGIC,DTB_VERSION,0,len(nodes),prop_count,24)
    body=header+records; total=len(body)+4; body=body[:8]+pu32(total)+body[12:]; blob=body+pu32(zlib.crc32(body)&0xffffffff)
    cfg.update(project='HalaOS',owner='HALA Academy',website='https://hala.edu.vn',target='STM32F103C8 Blue Pill',nodeCount=len(nodes),propertyCount=prop_count,dtb={'magic':DTB_MAGIC,'version':DTB_VERSION,'totalSize':total,'crc32':zlib.crc32(body)&0xffffffff})
    values=', '.join(f'0x{x:02X}' for x in blob)
    h=f'''/* SPDX-FileCopyrightText: 2026 HALA Academy */\n/* SPDX-License-Identifier: Apache-2.0 */\n/* GENERATED FILE - DO NOT EDIT MANUALLY. */\n#ifndef HALAOS_BOARD_CONFIG_H\n#define HALAOS_BOARD_CONFIG_H\n#define HALAOS_RAM_BASE 0x{cfg['memoryBase']:08X}u\n#define HALAOS_RAM_SIZE 0x{cfg['memorySize']:08X}u\n#define HALAOS_USART1_BASE 0x{cfg['usart1Base']:08X}u\n#define HALAOS_USART1_IRQ {cfg['usart1Irq']}u\n#define HALAOS_USART1_BAUD {cfg['baudrate']}u\n#define HALAOS_USART1_TX_PORT {pi(cfg['tx']['port'])}u\n#define HALAOS_USART1_TX_PIN {cfg['tx']['pin']}u\n#define HALAOS_USART1_RX_PORT {pi(cfg['rx']['port'])}u\n#define HALAOS_USART1_RX_PIN {cfg['rx']['pin']}u\n#define HALAOS_STATUS_LED_PORT {pi(cfg['led']['port'])}u\n#define HALAOS_STATUS_LED_PIN {cfg['led']['pin']}u\n#define HALAOS_STATUS_LED_ACTIVE_LOW {1 if cfg['led']['activeLow'] else 0}u\n#define HALAOS_DTB_MAGIC 0x{DTB_MAGIC:08X}u\n#define HALAOS_DTB_VERSION {DTB_VERSION}u\n#define HALAOS_DTB_SIZE {len(blob)}u\n#define HalaOS_RAM_BASE HALAOS_RAM_BASE\n#define HalaOS_RAM_SIZE HALAOS_RAM_SIZE\nextern const unsigned char g_halaos_compact_dtb[HALAOS_DTB_SIZE];\n#endif\n'''
    c=f'''/* SPDX-FileCopyrightText: 2026 HALA Academy */\n/* SPDX-License-Identifier: Apache-2.0 */\n/**\n * @file    board_config.c\n * @brief   Compact DTB sinh tự động từ board.dts.\n * @warning Không chỉnh sửa thủ công; chạy tools/dtsgen.py để sinh lại.\n * @note Project: HalaOS | Owner: HALA Academy | https://hala.edu.vn\n */\n#include "board_config.h"\nconst unsigned char g_halaos_compact_dtb[HALAOS_DTB_SIZE] __attribute__((section(".hala_dtb"),used)) = {{{values}}};\n'''
    a.out_h.parent.mkdir(parents=True,exist_ok=True);a.out_h.write_text(h,encoding='utf-8')
    out_c=a.out_c or a.out_h.with_name('board_config.c');out_c.write_text(c,encoding='utf-8')
    a.out_json.parent.mkdir(parents=True,exist_ok=True);a.out_json.write_text(json.dumps(cfg,indent=2)+'\n')
    od=a.out_dtb or a.out_h.with_name('board.dtb');od.write_bytes(blob)
    om=a.out_dtb_map or od.with_suffix('.dtb.map.json');om.write_text(json.dumps({'format':'HalaOS compact DTB v2 TLV','headerBytes':24,'recordOffset':24,'config':cfg},indent=2)+'\n')
    print(json.dumps({'status':'PASS','dtbBytes':len(blob),'nodes':len(nodes),'properties':prop_count,'config':cfg}))
    return 0
if __name__=='__main__': raise SystemExit(main())
