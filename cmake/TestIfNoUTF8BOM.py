#!/usr/bin/env python
import sys

def detectBOM(inFileName):
    file = open(inFileName, 'r')
    file.seek(0)
    head = map(ord, file.read(4))
    if head == [0x00, 0x00, 0xFE, 0xFF]:
        return "utf_32_be"
    elif head == [0xFF, 0xFE, 0x00, 0x00]:
        return "utf_32_le"
    elif head[:2] == [0xFE, 0xFF]:
        return "utf_16_be"
    elif head[:2] == [0xFF, 0xFE]:
        return "utf_16_le"
    elif head[:3] == [0xEF, 0xBB, 0xBF]:
        return "utf_8"
    else:
        return "unknown"

def main(argv = None):
    if argv is None:
        argv = sys.argv
    BOM = detectBOM(argv[1])
    if BOM != "utf_8":
        return 0
    else:
        print 'Detected byte-order mark ({0}) in file "{1}".'.format(
            BOM, argv[1])
        return 1
    
if __name__ == '__main__':
    sys.exit(main())
