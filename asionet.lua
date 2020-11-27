asionet_protocol = Proto("ASIONet",  "ASIONet Protocol")

message_id = ProtoField.int32("asionet.message_id", "messageId", base.DEC)
body_size = ProtoField.int32("asionet.body_size", "bodySize", base.DEC)
iv = ProtoField.bytes("asionet.iv", "iv")
data = ProtoField.bytes("asionet.data", "data")

asionet_protocol.fields = { message_id, body_size, iv, data }

function asionet_protocol.dissector(buffer, pinfo, tree)
  length = buffer:len()
  if length == 0 then return end

  pinfo.cols.protocol = asionet_protocol.name

  local subtree = tree:add(asionet_protocol, buffer(), "ASIONet Protocol Data")

  if length == 24  -- this works because with crypto it is not possible to have
                   -- a 24 byte body (due to alignment requirements) it could
                   -- screw up with unencrypted communication
  then
    local headerSubtree = subtree:add(asionet_protocol, buffer(), "Header")
    headerSubtree:add_le(message_id, buffer(0,4))
    headerSubtree:add_le(body_size, buffer(4,4))
    headerSubtree:add(iv, buffer(8,16))
    len = buffer(4,4):le_int()
  else
    local bodySubtree = subtree:add(asionet_protocol, buffer(), "Body")
    bodySubtree:add(data, buffer(0,length))
  end
end

local tcp_port = DissectorTable.get("tcp.port")
tcp_port:add(55512, asionet_protocol)
