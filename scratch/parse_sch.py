import os
import json

log_path = r"C:\Users\fml30\.gemini\antigravity-ide\brain\2527eb3b-1d2f-463b-bf34-8041c96a9ea4\.system_generated\logs\transcript.jsonl"

print(f"--- Searching logs at {log_path} ---")
if os.path.exists(log_path):
    with open(log_path, 'r', encoding='utf-8') as f:
        for line in f:
            try:
                obj = json.loads(line)
                content = obj.get('content', '')
                if '3.9' in content or '3,9' in content or '3.5' in content or '3,5' in content:
                    print(f"[{obj.get('source')}] {content[:200]}...")
            except Exception as e:
                pass
else:
    print("Log path does not exist.")
