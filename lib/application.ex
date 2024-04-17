defmodule Nextcast.Application do
  use Application
  require Logger

  defp configured_spec(name) do
    opts = Application.get_env(:nextcast, name)
    Logger.debug("#{name}, conf=#{inspect(opts)}, child_spec=#{inspect(apply(name, :child_spec, [opts]))}")
    {name, opts}
  end

  @impl true
  def start(_type, _args) do
    children = [
      {Task.Supervisor, name: :task_sup},
      configured_spec(Nextcast.ExtendedMetadata),
      configured_spec(Nextcast.RTPServer),
      configured_spec(Nextcast.DB),
      configured_spec(Nextcast.Track),
      configured_spec(Nextcast.StreamSupervisor),
      configured_spec(Nextcast.TCPServer)
    ]

    opts = [strategy: :one_for_one, name: Nextcast.Supervisor]
    Supervisor.start_link(children, opts)
  end

  def app_info do
    vsn = Application.spec(:nextcast, :vsn)
    node = node() |> Atom.to_string()
    {os_family, os_name} = :os.type()

    %{
      name: "Nextcast",
      vsn: vsn,
      compat_name: Nextcast.Compat.name,
      compat_vsn: Nextcast.Compat.vsn,
      node: node,
      os_family: os_family,
      os_name: os_name,
    }
  end
end
