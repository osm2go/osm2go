#!/usr/bin/env python3

import json
import urllib.request

with urllib.request.urlopen("https://github.com/openstreetmap/iD/raw/develop/data/discarded.json") as url:
	data = json.loads(url.read().decode())

	print("#pragma once\n")
	print("#include <array>\n")
	print("static const std::array<const char *,", len(data.keys()), "> discardable_tags = { {")

	for key in data.keys():
		print("\t\"" + key + "\",")

	print("}};")
