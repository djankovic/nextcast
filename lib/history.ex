defmodule Nextcast.History do
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
      rows
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
      rows
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
