defmodule Nextcast.MixProject do
  use Mix.Project

  def project do
    [
      app: :nextcast,
      version: "0.26.2",
      elixir: "~> 1.16",
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
      {:certifi, "~> 2.10"},
      {:cowboy, "~> 2.9"},
      {:plug_cowboy, "~> 2.6"},
      {:jason, "~> 1.4"},
      {:postgrex, "~> 0.17"},
      {:bcrypt_elixir, "~> 3.1"},
      {:credo, "~> 1.6", only: [:dev, :test], runtime: false}
    ]
  end
end
