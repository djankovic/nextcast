defmodule Nextcast.MixProject do
  use Mix.Project

  def project do
    [
      app: :nextcast,
      version: "0.15.1",
      elixir: "~> 1.14",
      start_permanent: Mix.env() == :prod,
      deps: deps(),
    ]
  end

  # Run "mix help compile.app" to learn about applications.
  def application do
    [
      extra_applications: [:logger, :inets, :eex],
      mod: {Nextcast.Application, []},
    ]
  end

  # Run "mix help deps" to learn about dependencies.
  defp deps do
    [
      {:certifi, "~> 2.10"},
      {:cowboy, "~> 2.9"},
      {:plug_cowboy, "~> 2.6"},
      {:jason, "~> 1.4"},
      {:postgrex, "~> 0.16.5"},
      {:bcrypt_elixir, "~> 3.0.1"},
      {:cowlib, "~> 2.12", override: true},
      {:ranch, "~> 2.1", override: true},
      {:credo, "~> 1.6", only: [:dev, :test], runtime: false}
    ]
  end
end
