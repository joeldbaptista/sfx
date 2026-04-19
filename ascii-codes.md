# ASCII codes

## Legend

| Abbr | Name | Description |
|------|------|-------------|
| NUL | Null | Originally used as padding; now often a string terminator in C |
| SOH | Start of Heading | Marks the start of a message header |
| STX | Start of Text | Marks the start of the message body |
| ETX | End of Text | Marks the end of the message body (Ctrl+C) |
| EOT | End of Transmission | Signals the end of a transmission (Ctrl+D on Unix) |
| ENQ | Enquiry | Request for a response from a remote station |
| ACK | Acknowledge | Positive acknowledgement |
| BEL | Bell | Triggers an audible alert/beep |
| BS  | Backspace | Moves cursor back one position |
| HT  | Horizontal Tab | Advances to the next tab stop |
| LF  | Line Feed | Moves to the next line (Unix newline) |
| VT  | Vertical Tab | Advances vertically to the next tab stop |
| FF  | Form Feed | Page break / eject page on printers |
| CR  | Carriage Return | Moves cursor to start of line |
| SO  | Shift Out | Switch to alternate character set |
| SI  | Shift In | Return to standard character set |
| DLE | Data Link Escape | Modifies meaning of following characters |
| DC1 | Device Control 1 | Commonly XON (resume transmission) |
| DC2 | Device Control 2 | Device-specific control |
| DC3 | Device Control 3 | Commonly XOFF (pause transmission) |
| DC4 | Device Control 4 | Device-specific control |
| NAK | Negative Acknowledge | Negative acknowledgement / error |
| SYN | Synchronous Idle | Used to maintain sync in sync transmission |
| ETB | End of Transmission Block | Ends a block of data |
| CAN | Cancel | Cancels previous data |
| EM  | End of Medium | End of usable storage medium |
| SUB | Substitute | Replaces an invalid/erroneous character |
| ESC | Escape | Introduces escape sequences (e.g., ANSI codes) |
| FS  | File Separator | Delimits files in a data stream |
| GS  | Group Separator | Delimits groups within a file |
| RS  | Record Separator | Delimits records within a group |
| US  | Unit Separator | Delimits units within a record |
| DEL | Delete | Originally punched all 7 holes on paper tape to erase |

## Table

**Note** 0–31 and 127 are control characters (non-printing).

| Dec | Hex  | Literal | Char / Key       |
|-----|------|---------|------------------|
| 0   | 0x00 | `\0`    | NUL (Ctrl+@)     |
| 1   | 0x01 |         | SOH (Ctrl+A)     |
| 2   | 0x02 |         | STX (Ctrl+B)     |
| 3   | 0x03 |         | ETX (Ctrl+C)     |
| 4   | 0x04 |         | EOT (Ctrl+D)     |
| 5   | 0x05 |         | ENQ (Ctrl+E)     |
| 6   | 0x06 |         | ACK (Ctrl+F)     |
| 7   | 0x07 | `\a`    | BEL (Ctrl+G)     |
| 8   | 0x08 | `\b`    | BS  (Ctrl+H)     |
| 9   | 0x09 | `\t`    | HT  (Ctrl+I/Tab) |
| 10  | 0x0A | `\n`    | LF  (Ctrl+J)     |
| 11  | 0x0B | `\v`    | VT  (Ctrl+K)     |
| 12  | 0x0C | `\f`    | FF  (Ctrl+L)     |
| 13  | 0x0D | `\r`    | CR  (Ctrl+M)     |
| 14  | 0x0E |         | SO  (Ctrl+N)     |
| 15  | 0x0F |         | SI  (Ctrl+O)     |
| 16  | 0x10 |         | DLE (Ctrl+P)     |
| 17  | 0x11 |         | DC1 (Ctrl+Q)     |
| 18  | 0x12 |         | DC2 (Ctrl+R)     |
| 19  | 0x13 |         | DC3 (Ctrl+S)     |
| 20  | 0x14 |         | DC4 (Ctrl+T)     |
| 21  | 0x15 |         | NAK (Ctrl+U)     |
| 22  | 0x16 |         | SYN (Ctrl+V)     |
| 23  | 0x17 |         | ETB (Ctrl+W)     |
| 24  | 0x18 |         | CAN (Ctrl+X)     |
| 25  | 0x19 |         | EM  (Ctrl+Y)     |
| 26  | 0x1A |         | SUB (Ctrl+Z)     |
| 27  | 0x1B | `\e`    | ESC (Ctrl+[)     |
| 28  | 0x1C |         | FS  (Ctrl+\\)    |
| 29  | 0x1D |         | GS  (Ctrl+])     |
| 30  | 0x1E |         | RS  (Ctrl+^)     |
| 31  | 0x1F |         | US  (Ctrl+_)     |
| 32  | 0x20 | ` `     | Space            |
| 33  | 0x21 | !       | !                |
| 34  | 0x22 | "       | "                |
| 35  | 0x23 | #       | #                |
| 36  | 0x24 | $       | $                |
| 37  | 0x25 | %       | %                |
| 38  | 0x26 | &       | &                |
| 39  | 0x27 | '       | '                |
| 40  | 0x28 | (       | (                |
| 41  | 0x29 | )       | )                |
| 42  | 0x2A | *       | *                |
| 43  | 0x2B | +       | +                |
| 44  | 0x2C | ,       | ,                |
| 45  | 0x2D | -       | -                |
| 46  | 0x2E | .       | .                |
| 47  | 0x2F | /       | /                |
| 48  | 0x30 | 0       | 0                |
| 49  | 0x31 | 1       | 1                |
| 50  | 0x32 | 2       | 2                |
| 51  | 0x33 | 3       | 3                |
| 52  | 0x34 | 4       | 4                |
| 53  | 0x35 | 5       | 5                |
| 54  | 0x36 | 6       | 6                |
| 55  | 0x37 | 7       | 7                |
| 56  | 0x38 | 8       | 8                |
| 57  | 0x39 | 9       | 9                |
| 58  | 0x3A | :       | :                |
| 59  | 0x3B | ;       | ;                |
| 60  | 0x3C | <       | <                |
| 61  | 0x3D | =       | =                |
| 62  | 0x3E | >       | >                |
| 63  | 0x3F | ?       | ?                |
| 64  | 0x40 | @       | @                |
| 65  | 0x41 | A       | A                |
| 66  | 0x42 | B       | B                |
| 67  | 0x43 | C       | C                |
| 68  | 0x44 | D       | D                |
| 69  | 0x45 | E       | E                |
| 70  | 0x46 | F       | F                |
| 71  | 0x47 | G       | G                |
| 72  | 0x48 | H       | H                |
| 73  | 0x49 | I       | I                |
| 74  | 0x4A | J       | J                |
| 75  | 0x4B | K       | K                |
| 76  | 0x4C | L       | L                |
| 77  | 0x4D | M       | M                |
| 78  | 0x4E | N       | N                |
| 79  | 0x4F | O       | O                |
| 80  | 0x50 | P       | P                |
| 81  | 0x51 | Q       | Q                |
| 82  | 0x52 | R       | R                |
| 83  | 0x53 | S       | S                |
| 84  | 0x54 | T       | T                |
| 85  | 0x55 | U       | U                |
| 86  | 0x56 | V       | V                |
| 87  | 0x57 | W       | W                |
| 88  | 0x58 | X       | X                |
| 89  | 0x59 | Y       | Y                |
| 90  | 0x5A | Z       | Z                |
| 91  | 0x5B | [       | [                |
| 92  | 0x5C | \\      | \\               |
| 93  | 0x5D | ]       | ]                |
| 94  | 0x5E | ^       | ^                |
| 95  | 0x5F | _       | _                |
| 96  | 0x60 | `` ` `` | `                |
| 97  | 0x61 | a       | a                |
| 98  | 0x62 | b       | b                |
| 99  | 0x63 | c       | c                |
| 100 | 0x64 | d       | d                |
| 101 | 0x65 | e       | e                |
| 102 | 0x66 | f       | f                |
| 103 | 0x67 | g       | g                |
| 104 | 0x68 | h       | h                |
| 105 | 0x69 | i       | i                |
| 106 | 0x6A | j       | j                |
| 107 | 0x6B | k       | k                |
| 108 | 0x6C | l       | l                |
| 109 | 0x6D | m       | m                |
| 110 | 0x6E | n       | n                |
| 111 | 0x6F | o       | o                |
| 112 | 0x70 | p       | p                |
| 113 | 0x71 | q       | q                |
| 114 | 0x72 | r       | r                |
| 115 | 0x73 | s       | s                |
| 116 | 0x74 | t       | t                |
| 117 | 0x75 | u       | u                |
| 118 | 0x76 | v       | v                |
| 119 | 0x77 | w       | w                |
| 120 | 0x78 | x       | x                |
| 121 | 0x79 | y       | y                |
| 122 | 0x7A | z       | z                |
| 123 | 0x7B | {       | {                |
| 124 | 0x7C | \|      | \|               |
| 125 | 0x7D | }       | }                |
| 126 | 0x7E | ~       | ~                |
| 127 | 0x7F |         | DEL              |
