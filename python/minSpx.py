import os
import subprocess
class Evaluation:        
    def run_superpixel_evaluation(ext, img_path, gt_path, spx_map_path, save_path, min_spx_size):
        # Create the save directory if it doesn't exist
        os.makedirs(save_path, exist_ok=True)
        
        # Prepare the command with the given parameters
        params = [
            "--ext", ext,
            "--eval", "10",
            "--img", img_path,
            "--gt", gt_path,
            "--rmsize", str(min_spx_size),
            "--save", save_path,
            "--label", spx_map_path
        ]
        
        command = ["./bin/main"] + params
        
        # Run the command
        result = subprocess.run(command, capture_output=True, text=True)
        
        # Print the output and error (if any)
        print("Output:")
        print(result.stdout)
        if result.stderr:
            print("Error:")
            print(result.stderr)

