---
schema: knowledge/v1
id: delivery-income-is-booked-when-the-vehicle-leaves
kind: knowledge
class: fact
title: "Cargo delivery income reaches company money when the vehicle leaves the station, not when it unloads"
description: "When an experiment or a bot measures income per delivery."
status: candidate
epistemics: observed
date: 2026-11-16
tags: [economy-and-companies, towns-industries-stations]
about: src/economy.cpp
route: platform
provenance:
  origin: authored
  derived_from:
    - file:src/economy.cpp
    - file:src/vehicle.cpp
relations:
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/towns-industries-stations/README.md
  - type: relates_to
    target: file:.ai/repo/workflows/substrate/economy-and-companies/README.md
---

# Statement

`src/economy.cpp:CargoPayment::PayFinalDelivery` only accumulates the route profit; the destructor of
`CargoPayment` moves it into the company's money, and it runs from `src/vehicle.cpp:Vehicle::LeaveStation`.
Sample money after departure.

# Review

Authored from a read of the source at f069e8f0f5 (issue I0055); awaiting the owner's review.
`majordomus knowledge promote delivery-income-is-booked-when-the-vehicle-leaves` with evidence, or `majordomus knowledge reject delivery-income-is-booked-when-the-vehicle-leaves --reason ...`.
