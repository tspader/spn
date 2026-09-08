FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends {{.packages}} && rm -rf /var/lib/apt/lists/*
{{for .artifacts}}
COPY --from={{.name}} / {{.path}}
{{end}}
{{for .setups}}
RUN {{.steps}}
{{end}}
