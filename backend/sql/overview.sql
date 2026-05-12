SELECT
    st.id,
    st.name,
    st.keepalive,
    st.latitude,
    st.longitude,
    COALESCE(SUM(sp.state = 'free'), 0) AS free_spaces,
    COALESCE(SUM(sp.state = 'used'), 0) AS used_spaces
FROM stations st
LEFT JOIN spaces sp ON sp.station = st.id
GROUP BY st.id, st.name, st.keepalive;