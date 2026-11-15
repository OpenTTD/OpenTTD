---
id: majordomus.deployment-contract
version: 2
kind: rule
title: A deployment is described once, and every artifact built from it is generated from it
description: A deployment of the executable is a canonical object of the layer against a closed contract; the container definition, its ignore file and the files of the provider it names are projections of it, never a second description.
statement: A deployment is one object of the layer, valid against the deployment contract and carrying no credential; it names no provider unless it names one the contract knows, at an https address; the container image definition, its ignore file and every file of its provider are generated from it and are never authoritative.
status: active
class: blocking
depends_on: [majordomus.ai-layout-integrity@1, majordomus.projection-integrity@1]
tags: [deployment, schema, projection]

x-majordomus:
  validator: deployments
  category: deployment
  enforced_by: [doctor, watch]
  exit_code: 10
  tests: [test/cases/84_deployment_contract.sh, test/cases/86_deployment_projections.sh, test/cases/3820_a_deployment_renders_its_providers_files.sh]
---

# Rationale

A `Dockerfile`, its ignore file and a provider's Procfile written by hand restate what the repository
already knows — the port the server binds, the routes it registers, the binary the
workspace names, the paths a build reads — and every one of those pairs drifts the first
time one half changes. The failure is silent and it is discovered in production: an image
that runs a binary under a name nobody builds any more, a health check pointed at a route
that was renamed, a machine sized for a memory figure a release outgrew.

The repository already refuses that shape everywhere else. A capability is declared once
and projected into the CLI, HTTP, MCP, OpenAPI, the documentation and the site, and
`generate --check` fails when a projection goes stale. A deployment is the same kind of
fact and arrives the same way, or it does not arrive.

The second reason is money and disclosure. A deployment states how much runs and for how
long, so the description has to be somewhere a reviewer reads rather than in a provider
dashboard nobody diffs. And it must never hold the credential that authorises it: the
contract has no field for one, and a token smuggled into a field that does exist is
refused here.

# Required behaviour

A deployment is a `*.yaml` object under the layer's deployments section, valid against
`share/schemas/majordomus/deployment/deployment.v1.schema.json` (`deployment/v1`). It states the application, the
package and binary shipped, the port and the interface the process listens on, the
liveness and readiness routes, the resources, the machine count, the region and the build
inputs — each exactly once.

A deployment names no hosting provider unless it names one the contract knows (ADR 0219).
Today that is `gigalixir`; another name is refused like any other value outside the
contract. A named provider states its application, its stack and the URL the deployment
answers on, and that URL is `https://`. A hosted process binds every interface, so a
provider beside `listen.interface: loopback` is refused. The credential that authorises a
push to the provider is never a field: the contract has no key for one, and a token
smuggled into a field that does exist is refused below.

Keys are closed by `share/allow/deployment.txt`, generated from the schema: an unknown key
is an error, never carried. Two objects may not claim one identity. `min_running` may not
exceed `count`. A health route is a path, not a URL. The port is unprivileged, because the
deployed process does not run as root.

Every artifact built from it — the container image definition, its ignore file and every
file the named provider reads, under `deploy/<provider>/` (for Gigalixir the Procfile and
the inline buildpack's `bin/detect`, `bin/compile` and `bin/release`) — is
written by `majordomus generate` from this object, carries a provenance
header naming its source and its regeneration command, and is compared by
`generate --check`. A contributor who edits a generated artifact is told which file to
change instead.

# Failure behaviour

A violation is a `FAIL` finding under the category `deployment`, naming the object, the
key, the value found and the correction; the command that found it exits 10, and under
`watch` it is drift and the command exits 11.

# Verification

`mj_validate_deployments` decides it, dispatched from `doctor, watch`. The behavioural
case `test/cases/84_deployment_contract.sh` proves it by mutation: an unknown key, an
unknown provider name, a provider URL that is not https, a provider beside a loopback
interface, an unknown schema version, a missing required field, a privileged port, a
`min_running` above `count`, a health route that is not a path, a duplicate identity and
a credential in a field that exists — each refused by name; and a valid object, with and
without a known provider, accepted. `test/cases/86_deployment_projections.sh` holds the
generated files of the Majordomus repository's own object, the provider's among them, to their
header, their port and `generate --check`, and
`test/cases/3820_a_deployment_renders_its_providers_files.sh` renders a provider's files
from a fixture object.
