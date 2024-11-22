defmodule Nextcast.MixProject do
  use Mix.Project

  def project do
    [
      app: :nextcast,
      version: "0.30.1",
      elixir: "~> 1.17",
      start_permanent: Mix.env() == :prod,
      test_paths: ["lib"],
      deps: deps(),
    ]
  end

  defp extra_applications(:dev), do: [:wx, :observer]
  defp extra_applications(_), do: []
  def application do
    [
      extra_applications: extra_applications(Mix.env()) ++ [:logger, :inets, :eex, :xmerl, :runtime_tools, :crypto],
      mod: {Nextcast.Application, []},
    ]
  end

  defp deps do
    [
      {:cowboy, "~> 2.9"},
      {:ex_webrtc, "~> 0.6.2"},
      {:jason, "~> 1.4"},
      {:plug_cowboy, "~> 2.6"},
      {:postgrex, "~> 0.17"},
      {:zoneinfo, "~> 0.1.0"},
      {:credo, "~> 1.6", only: [:dev, :test], runtime: false}
    ]
  end
end
