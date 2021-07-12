.DEFAULT_GOAL := help

CREATIVE_INDEX := file://$(abspath $(dir $(lastword $(MAKEFILE_LIST)))/assets/current/index.html)

FILES_IN_FOLDER = $(shell find assets/current -type f)

DISTRO_FILE = $(abspath ./_distro/creative.zip)

# From https://marmelab.com/blog/2016/02/29/auto-documented-makefile.html
.PHONY: help
help:
	@grep -E '^[a-zA-Z_-_.]+:.*?## .*$$' $(MAKEFILE_LIST) | sort | awk 'BEGIN {FS = ":.*?## "}; {printf "\033[36m%-30s\033[0m %s\n", $$1, $$2}'

.PHONY: dist.size
dist.size:  ## Check the HTML5 size
	@du -hs assets/current

.PHONY: dist.build
dist.build:  ## Build the distribution
	@mkdir -p ./_distro
	cd assets/current && zip -FSr $(DISTRO_FILE) . -x ./.\*

.PHONY: dist.clean
dist.clean:  ## Clean the distribution
	@rm $(DISTRO_FILE)

.PHONY: html.open
html.open:  ## Open the documentation in the default browser
	python -m webbrowser $(CREATIVE_INDEX)

dev.debug:  ## Debug
	@echo $(DISTRO_FILE)
	@echo $(FILES_IN_FOLDER)

woff.clean:  ## Clean woffstrip
	cd tools/woffstrip && $(MAKE) clean
