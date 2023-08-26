# SPDX-FileCopyrightText: 2016-2022 by pi-lar GmbH
# SPDX-License-Identifier: OSL-3.0

'''
To evaluate the .puml file generated by this script use www.plantuml.com/plantuml
'''

from pathlib import Path
import os
import subprocess
import plantuml


def make_svg(puml_txt:str,filename:str,auto_open=True):
    path = Path(os.path.join(Path(__file__).parent.absolute(),"..","build","puml"))
    path.mkdir(parents=True, exist_ok=True)
    with open(os.path.join(path,filename),"w+") as f:
        t = f.write(puml_txt)

    pu = plantuml.PlantUML(url="http://www.plantuml.com/plantuml/svg/")

    img = pu.processes(puml_txt)
    svg_path = os.path.join(path,"states.svg")
    with open(svg_path,"wb+") as f:
        t = f.write(img)

    if auto_open:
        subprocess.run(["firefox", svg_path])
