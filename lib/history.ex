defmodule Nextcast.History do
  defp map_get_rows(rows) do
    Enum.map(rows, fn [time, query] ->
      pretty_track = Nextcast.ExtendedMetadata.parse(query) |> Nextcast.ExtendedMetadata.to_query()
      {time, pretty_track}
    end)
  end

  def get(stream, limit \\ 20) do
    with {:ok, %{rows: rows}} <-
           Nextcast.DB.query(
             """
             select time, raw_track
             from history
             where stream = $1
             order by time desc
             limit $2
             """,
             ["#{stream}", limit]
           ) do
      map_get_rows(rows)
    else
      _ -> []
    end
  end

  def get_since(stream, time) do
    with {:ok, %{rows: rows}} <-
           Nextcast.DB.query(
             """
             select time, raw_track
             from history
             where stream = $1 and time > $2
             order by time desc
             """,
             [stream, time]
           ) do
      map_get_rows(rows)
    else
      _ -> []
    end
  end

  def insert(stream, items) do
    Nextcast.DB.query(
      """
      insert into history
      (stream, time, raw_track)
      values
      #{1..Enum.count(items)//1 |> Enum.map(fn i -> "($1, $#{i * 2}, $#{i * 2 + 1})" end) |> Enum.join(",")}
      on conflict do nothing
      """,
      ["#{stream}"] ++ Enum.flat_map(items, &Tuple.to_list/1)
    )
  end

  def insert(stream, time, raw_track) do
    insert(stream, [{time, raw_track}])
  end

  def remove(stream, time) do
    Nextcast.DB.query(
      """
      delete from history
      where stream = $1 and time = $2
      """,
      ["#{stream}", time]
    )
  end
end
