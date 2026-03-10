Import("env")

# This function runs a terminal command to upload the file system
def upload_fs(source, target, env):
    print("\n--- Auto-Uploading LittleFS Image ---\n")
    env.Execute("pio run --target uploadfs")

# Attach the function to run AFTER the main 'upload' finishes
env.AddPostAction("upload", upload_fs)