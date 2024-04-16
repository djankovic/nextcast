defmodule Nextcast.MixProject do
  use Mix.Project

  def project do
    [
      app: :nextcast,
      version: "0.19.14",
      elixir: "~> 1.16",
      start_permanent: Mix.env() == :prod,
      deps: deps(),
    ]
  end

  # Run "mix help compile.app" to learn about applications.
  def application do
    [
      extra_applications: [:logger, :inets, :eex, :xmerl],
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
      {:postgrex, "~> 0.17"},
      {:bcrypt_elixir, "~> 3.1"},
      {:credo, "~> 1.6", only: [:dev, :test], runtime: false}
    ]
  end
end
