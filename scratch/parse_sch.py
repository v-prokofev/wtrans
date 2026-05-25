import re

def trace_ve1_net():
    filepath = r"c:\src\VNIIM\wtrans\pcb\LOOP_DAC.SchDoc"
    with open(filepath, 'rb') as f:
        data = f.read()
    
    records = data.split(b'|')
    # Let's find VU1 pin 7 and see its PinUniqueId or Net connection.
    # In Altium, pins are connected to nets via Pin records or Net Labels.
    # Let's look for records that mention VU1 or pin 7 or VE1.
    for i, r in enumerate(records):
        if b'VE1' in r or b'VU1' in r:
            try:
                text = r.decode('ascii', errors='replace').strip()
                if 'VE1' in text or 'VU1' in text:
                    print(f"Record {i}: {text}")
                    # Print surrounding records
                    for j in range(max(0, i-3), min(len(records), i+4)):
                        print(f"  [{j}]: {records[j].decode('ascii', errors='replace').strip()}")
                    print("-" * 50)
            except Exception as e:
                pass

trace_ve1_net()
