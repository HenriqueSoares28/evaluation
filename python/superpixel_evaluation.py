import os
import subprocess

class SuperpixelEvaluator:
    def __init__(self, ext, img_path, gt_path, spx_map_path, save_path, min_spx_size):
        self.ext = ext
        self.img_path = img_path
        self.gt_path = gt_path
        self.spx_map_path = spx_map_path
        self.save_path = save_path
        self.min_spx_size = min_spx_size

    def create_save_directory(self):
        os.makedirs(self.save_path, exist_ok=True)

    def build_command(self):
        params = [
            "--ext", self.ext,
            "--eval", "10",
            "--img", self.img_path,
            "--gt", self.gt_path,
            "--rmsize", str(self.min_spx_size),
            "--save", self.save_path,
            "--label", self.spx_map_path
        ]
        return ["./bin/main"] + params

    def run(self):
        self.create_save_directory()
        command = self.build_command()
        
        result = subprocess.run(command, capture_output=True, text=True)
        
        print("Output:")
        print(result.stdout)
        if result.stderr:
            print("Error:")
            print(result.stderr)
