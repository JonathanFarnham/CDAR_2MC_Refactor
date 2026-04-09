Import("env")
import time

def upload_fs(source, target, env):
    print("\n" + "="*50)
    print(" C++ Upload Complete!")
    print(" Waiting 2 seconds for COM port to reset...")
    print("="*50 + "\n")
    
    # This delay prevents the "Port Access Denied" / "COM busy" error
    time.sleep(2) 
    
    print("\n--- Auto-Uploading LittleFS Image ---\n")
    # Trigger the file system upload
    env.Execute("pio run --target uploadfs")
    print("\n--- All Uploads Complete! ---\n")

# Attach this function to run automatically after the main 'upload' task
env.AddPostAction("upload", upload_fs)