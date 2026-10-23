"""Assemble passive MQTT JSONL captures; no broker connection or device commands."""
import argparse
import json
import os
from pathlib import Path

def assemble(lines, topic=None):
    runs = {}
    rejected = 0
    for line in lines:
        try:
            message = json.loads(line)
            if topic and message.get('topic') != topic:
                continue
            payload = message.get('payload', message)
            if isinstance(payload, str):
                payload = json.loads(payload)
            result = payload.get('diagnostic_result')
            if result is None:
                continue
            if isinstance(result, str):
                result = json.loads(result)
            packet = bytes.fromhex(result['packet_hex'])
            if len(packet) != 24 or packet[0] != 0xd1 or packet[1] not in (1, 2):
                raise ValueError('Invalid packet')
            run_id = int.from_bytes(packet[2:6], 'little')
            source = message.get('topic', '<raw-input>')
            protocol = packet[1]
            run = runs.setdefault((source, protocol, run_id), {'topic': source, 'protocol': protocol, 'run_id': run_id, 'cases': {}, 'summary': None,
                                           'persisted_observed': False, 'replay_observed': False})
            run['persisted_observed'] |= bool(packet[7] & 1)
            run['replay_observed'] |= bool(packet[7] & 4)
            if packet[6] == 255:
                if packet[8] != 16:
                    raise ValueError('Unexpected suite length')
                run['summary'] = result
            elif packet[6] < 16 and packet[8] == packet[6] + 1:
                run['cases'][packet[6]] = result
            else:
                raise ValueError('Invalid case index')
        except (ValueError, KeyError, TypeError, AttributeError):
            rejected += 1
    for run in runs.values():
        run['checksum_matches'] = False
        if len(run['cases']) == 16 and run['summary']:
            record = bytearray(b'SHTD' + bytes([run['protocol'], 16, 1, 0]))
            record += run['run_id'].to_bytes(4, 'little') + bytes(4)
            for index in range(16):
                record += bytes.fromhex(run['cases'][index]['packet_hex'])[8:24]
            checksum = 2166136261
            for byte in record:
                checksum = ((checksum ^ byte) * 16777619) & 0xffffffff
            expected = int.from_bytes(bytes.fromhex(run['summary']['packet_hex'])[12:16], 'little')
            run['checksum_matches'] = checksum == expected
        run['complete'] = bool(run['summary'] and run['summary'].get('suite_finished') and
                               len(run['cases']) == 16 and run['checksum_matches'])
        run['missing_case_ids'] = [i + 1 for i in range(16) if i not in run['cases']]
        run['cases'] = [run['cases'][i] for i in sorted(run['cases'])]
    return {'runs': [runs[i] for i in sorted(runs)], 'rejected_lines': rejected}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--topic', help='Exact device topic when the capture contains multiple devices')
    args = parser.parse_args()
    result = assemble(args.input.read_text().splitlines(), args.topic)
    descriptor = os.open(args.output, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(descriptor, 'w') as stream:
        json.dump(result, stream, indent=2)
        stream.write('\n')
    print(json.dumps({'runs': len(result['runs']), 'complete_runs': sum(r['complete'] for r in result['runs'])}))

if __name__ == '__main__':
    main()
