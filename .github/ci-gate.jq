(.build.result == "success" or .cold.result == "success")
and .package.result == "success"
and (.canary.result == "success" or .canary.result == "skipped")
