# RESEARCH-ONLY: the battle brain (research/battle-brain.md). A learned surrogate of the LAN battle:
# from every recorded PrismaticAI result (its settings -> its log value share of the winner), an
# ensemble of small Axon networks compiled by EXLA predicts what an untried setting would score,
# with the ensemble's spread as its uncertainty. It scores tens of thousands of candidate settings
# from the genetic algorithm's own gene space in one batched EXLA call and proposes, alternately,
# the best by lower confidence bound (mean - kappa * spread: safe) and by upper confidence bound
# (mean + kappa * spread: explore). It never changes a contestant: proposals are an input for the battle's GA
# (/api/tuning) and the live tuner (rhai/prismatic_tune.rhai).
#
# Input  (argv[0]): a CSV of edition,contestant,settings,share (tools/research/battle-brain/brain).
# Input  (argv[1]): the GA's gene space as JSON {"gene": [min, max, step]} (/api/tuning "space").
# Output (argv[2]): the proposals JSON (schema battle-brain/v1).
Application.ensure_all_started(:exla)
Nx.default_backend(EXLA.Backend)

defmodule Brain do
  @members 5
  @epochs 400
  @candidates 50_000
  @kappa 1.0
  @top 8

  def parse_settings(""), do: %{}
  def parse_settings(s) do
    s |> String.split(",", trim: true)
    |> Enum.flat_map(fn kv ->
      case String.split(kv, "=", parts: 2) do
        [k, v] -> case Float.parse(String.trim(v)) do {f, _} -> [{String.trim(k), f}]; :error -> [] end
        _ -> []
      end
    end) |> Map.new()
  end

  # A row of features in [0, 1]: each gene scaled by its range; a gene the setting does not name
  # takes the space's midpoint (the unknown default), flagged so the net can tell them apart.
  def features(settings, genes, space) do
    Enum.flat_map(genes, fn g ->
      [lo, hi, _] = space[g]
      case Map.fetch(settings, g) do
        {:ok, v} -> [min(max((v - lo) / max(hi - lo, 1.0e-9), 0.0), 1.0), 1.0]
        :error -> [0.5, 0.0]
      end
    end)
  end

  def model(n_in) do
    Axon.input("x", shape: {nil, n_in})
    |> Axon.dense(32, activation: :tanh)
    |> Axon.dropout(rate: 0.1)
    |> Axon.dense(32, activation: :tanh)
    |> Axon.dense(1)
  end

  def train(x, y, seed) do
    n = elem(Nx.shape(x), 0)
    key = Nx.Random.key(seed)
    {idx, _} = Nx.Random.randint(key, 0, n, shape: {n})  # a bootstrap sample per member
    xb = Nx.take(x, idx); yb = Nx.take(y, idx)
    data = Stream.repeatedly(fn -> {%{"x" => xb}, yb} end)
    model(elem(Nx.shape(x), 1))
    |> Axon.Loop.trainer(:mean_squared_error, Polaris.Optimizers.adam(learning_rate: 0.01))
    |> Axon.Loop.run(data, Axon.ModelState.empty(), epochs: 1, iterations: @epochs, compiler: EXLA, log: 0)
  end

  def candidates(genes, space, n, seed) do
    :rand.seed(:exsss, {seed, seed + 1, seed + 2})
    for _ <- 1..n do
      Map.new(genes, fn g ->
        [lo, hi, step] = space[g]
        steps = max(round((hi - lo) / max(step, 1)), 0)
        {g, lo + step * :rand.uniform(steps + 1) - step}
      end)
    end
  end

  def run(csv, space_json, out) do
    space = space_json |> File.read!() |> Jason.decode!() |> Map.get("space", %{}) |> Map.new(fn {k, v} -> {k, Enum.map(v, &(&1 * 1.0))} end)
    genes = space |> Map.keys() |> Enum.sort()
    rows = csv |> File.stream!() |> Enum.drop(1) |> Enum.map(&parse_csv/1) |> Enum.reject(&is_nil/1)
    distinct = rows |> Enum.map(& &1.settings) |> Enum.uniq() |> length()
    x = rows |> Enum.map(&features(parse_settings(&1.settings), genes, space)) |> Nx.tensor(type: :f32)
    y = rows |> Enum.map(&[:math.log(max(&1.share, 1.0e-6))]) |> Nx.tensor(type: :f32)
    {train_us, members} = :timer.tc(fn -> for s <- 1..@members, do: train(x, y, s) end)
    model = model(elem(Nx.shape(x), 1))
    {_init, predict} = Axon.build(model, compiler: EXLA)
    cands = candidates(genes, space, @candidates, length(rows))
    xc = cands |> Enum.map(&features(&1, genes, space)) |> Nx.tensor(type: :f32)
    {score_us, preds} = :timer.tc(fn -> members |> Enum.map(&predict.(&1, xc)) |> Nx.stack() |> Nx.squeeze(axes: [2]) end)
    mean = Nx.mean(preds, axes: [0]); std = Nx.standard_deviation(preds, axes: [0])
    ucb = Nx.add(mean, Nx.multiply(std, @kappa))
    lcb = Nx.subtract(mean, Nx.multiply(std, @kappa))
    # The proposals alternate a safe pick and an explorer: the best lower confidence bound (where
    # the ensemble agrees the setting is good) and the best upper one (where it might be). The GA
    # takes the first two, so each generation gets one of each; an explorer that ends last is culled
    # by the GA's last-place penalty, a safe pick keeps the population from collapsing.
    safe = lcb |> Nx.argsort(direction: :desc) |> Nx.slice([0], [@top]) |> Nx.to_flat_list()
    # The explorer is constrained: only candidates whose lower bound clears the floor (a ratio to
    # the default, $BRAIN_EXPLORE_FLOOR, default 0.9) may be picked, so exploring never proposes a
    # setting the ensemble already expects to lose badly (g66.brainExplore: short air routes, last
    # once in edition 69577ebe). Where no candidate clears it, the explorer falls back to the safe order.
    floor = :math.log(String.to_float(System.get_env("BRAIN_EXPLORE_FLOOR", "0.9")))
    ok = Nx.greater_equal(lcb, floor)
    bold = if Nx.to_number(Nx.any(ok)) == 1 do
      Nx.select(ok, ucb, Nx.Constants.neg_infinity(:f32)) |> Nx.argsort(direction: :desc) |> Nx.slice([0], [@top]) |> Nx.to_flat_list()
    else
      safe
    end
    order = Enum.zip(safe, bold) |> Enum.flat_map(fn {a, b} -> [{a, "safe"}, {b, "explore"}] end)
            |> Enum.uniq_by(&elem(&1, 0)) |> Enum.take(@top)
    # In-sample fit of the ensemble mean, as a sanity figure (not a validation).
    fit = members |> Enum.map(&predict.(&1, x)) |> Nx.stack() |> Nx.mean(axes: [0])
    r2 = 1 - Nx.to_number(Nx.sum(Nx.pow(Nx.subtract(y, fit), 2))) / max(Nx.to_number(Nx.sum(Nx.pow(Nx.subtract(y, Nx.mean(y)), 2))), 1.0e-9)
    proposals = for {i, kind} <- order do
      c = Enum.at(cands, i)
      %{settings: c |> Enum.sort() |> Enum.map_join(",", fn {k, v} -> "#{k}=#{round(v)}" end), kind: kind,
        predicted_share: Float.round(:math.exp(Nx.to_number(mean[i])), 4),
        uncertainty: Float.round(Nx.to_number(std[i]), 4)}
    end
    best_seen = rows |> Enum.max_by(& &1.share, fn -> nil end)
    doc = %{schema: "battle-brain/v1", target: System.get_env("BRAIN_TARGET", "raw: log value share of the winner"), rows: length(rows), distinct_settings: distinct, genes: genes,
      members: @members, epochs: @epochs, candidates: @candidates, kappa: @kappa, explore_floor: :math.exp(floor),
      backend: "EXLA #{inspect(EXLA.Client.get_supported_platforms() |> Map.keys())}",
      train_ms: div(train_us, 1000), score_ms: div(score_us, 1000), in_sample_r2: Float.round(r2, 3),
      enough_data: distinct >= 2 * length(genes),
      best_seen: best_seen && %{settings: best_seen.settings, share: best_seen.share},
      proposals: proposals}
    File.write!(out, Jason.encode!(doc, pretty: true))
    IO.puts("battle-brain: #{length(rows)} rows, #{distinct} distinct settings, #{length(genes)} genes; trained #{@members} nets in #{div(train_us, 1000)} ms, scored #{@candidates} candidates in #{div(score_us, 1000)} ms on EXLA; in-sample R2 #{Float.round(r2, 3)}#{if doc.enough_data, do: "", else: " (too few distinct settings: proposals explore)"}")
  end

  defp parse_csv(line) do
    # edition,contestant,"settings",share  (settings may hold commas, inside quotes)
    case Regex.run(~r/^([^,]*),([^,]*),"?(.*?)"?,([0-9.eE+-]+)\s*$/, String.trim(line)) do
      [_, ed, k, s, share] -> %{edition: ed, contestant: k, settings: s, share: String.to_float(normalize(share))}
      _ -> nil
    end
  end
  defp normalize(s), do: if(String.contains?(s, "."), do: s, else: s <> ".0")
end

[csv, space, out] = System.argv()
Brain.run(csv, space, out)
