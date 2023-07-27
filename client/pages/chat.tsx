import Head from 'next/head';
import Header from '../components/header';
import { Avatar } from '@mui/material';
import { useCallback, useEffect, useReducer, useRef } from 'react';


function genRandomName() {
  const number = Math.floor(Math.random() * 999999);
  return `user-${number}`
}

function getStoredUser(): User {
  const user = localStorage.getItem('servertech_user')
  if (user) {
    return JSON.parse(user)
  } else {
    return { id: crypto.randomUUID(), username: genRandomName() }
  }
}

// Copied from MUI Avatar docs
function stringToColor(string: string) {
  let hash = 0;
  let i;

  /* eslint-disable no-bitwise */
  for (i = 0; i < string.length; i += 1) {
    hash = string.charCodeAt(i) + ((hash << 5) - hash);
  }

  let color = '#';

  for (i = 0; i < 3; i += 1) {
    const value = (hash >> (i * 8)) & 0xff;
    color += `00${value.toString(16)}`.slice(-2);
  }
  /* eslint-enable no-bitwise */

  return color;
}

function formatDate(date: number): string {
  return new Intl.DateTimeFormat(undefined, {
    dateStyle: 'medium',
    timeStyle: 'short'
  }).format(new Date(date))
}

const NameAvatar = ({ name }: { name: string }) => {
  return <Avatar sx={{
    bgcolor: stringToColor(name),
  }}>{name[0]}</Avatar>
}

const RoomEntry = ({
  id,
  name,
  lastMessage,
  selected,
  onClick
}: {
  id: string,
  name: string,
  lastMessage: string,
  selected: boolean,
  onClick: (roomId: string) => void
}) => {
  const cardStyle = selected ? { backgroundColor: 'var(--boost-light-grey)' } : {}
  return (
    <div className='flex pt-2 pl-3 pr-3 cursor-pointer overflow-hidden' onClick={() => onClick(id)}>
      <div className='flex-1 flex p-4 rounded-lg overflow-hidden' style={cardStyle}>
        <div className='pr-3 flex flex-col justify-center'>
          <NameAvatar name={name} />
        </div>
        <div className='flex-1 flex flex-col overflow-hidden'>
          <p className='m-0 font-bold pb-2'>{name}</p>
          <p className='m-0 overflow-hidden text-ellipsis whitespace-nowrap'>{lastMessage}</p>
        </div>
      </div>
    </div>
  )
}

const OtherUserMessage = ({ username, content, timestamp }: { username: string, content: string, timestamp: number }) => {
  return (
    <div className='flex flex-row pt-3 pb-3'>
      <div className='pr-5 flex flex-col justify-end'>
        <NameAvatar name={username} />
        <p className='m-0 pt-1 text-sm'>{username}</p>
      </div>
      <div className='flex-[6] bg-white rounded-lg pt-4 pl-5 pr-2'>
        <p className='m-0'>{content}</p>
        <p className='m-0 pb-2 pr-2 text-xs text-right' style={{ color: 'var(--boost-grey)' }}>{formatDate(timestamp)}</p>
      </div>
      <div className='flex-[3]'>
      </div>
    </div>
  )
}

const MyMessage = ({ content, timestamp }: { content: string, timestamp: number }) => {
  return (
    <div className='flex flex-row pt-3 pb-3'>
      <div className='flex-[4]'>
      </div>
      <div className='flex-[6] bg-green-100 rounded-lg pt-4 pl-5 pr-2'>
        <p className='m-0'>{content}</p>
        <p className='m-0 pb-2 pr-2 text-xs text-right' style={{ color: 'var(--boost-grey)' }}>{formatDate(timestamp)}</p>
      </div>
    </div>
  )
}

type User = {
  id: string
  username: string
}

type Message = {
  id: string
  user: User
  content: string
  timestamp: number // miliseconds since epoch
}

type Room = {
  id: string
  name: string
  messages: Message[]
  hasMoreMessages: boolean
}

type State = {
  currentUser: User | null
  loading: boolean
  rooms: Record<string, Room> // id => room
  currentRoomId: string | null
}

type Action = {
  type: 'set_initial_state',
  payload: {
    currentUser: User,
    rooms: Room[]
  }
} | {
  type: 'add_messages',
  payload: {
    roomId: string
    messages: Message[]
  }
} | {
  type: 'set_current_room',
  payload: {
    roomId: string
  }
}

const Message = ({
  userId,
  username,
  content,
  timestamp,
  currentUserId
}: {
  userId: string,
  username: string,
  content: string,
  timestamp: number,
  currentUserId: string
}) => {
  if (userId === currentUserId) {
    return <MyMessage content={content} timestamp={timestamp} />
  } else {
    return <OtherUserMessage content={content} timestamp={timestamp} username={username} />
  }
}

function reducer(state: State, action: Action): State {
  const { type, payload } = action
  switch (type) {
    case 'set_initial_state':
      const roomsById = {}
      for (let room of payload.rooms)
        roomsById[room.id] = room
      return {
        currentUser: payload.currentUser,
        loading: false,
        rooms: roomsById,
        currentRoomId: payload.rooms.length > 0 ? payload.rooms[0].id : null
      }
    case 'add_messages':
      // Find the room
      const room = state.rooms[payload.roomId]
      if (!room)
        return state // We don't know what the server is talking about

      // Add the new messages to the array
      return {
        ...state,
        rooms: {
          ...state.rooms,
          [payload.roomId]: {
            ...room,
            // TODO: this looks like too modern
            messages: payload.messages.toReversed().concat(room.messages)
          }
        }
      }
    case 'set_current_room':
      return {
        ...state,
        currentRoomId: payload.roomId
      }


    default: return state
  }
}

export default function Home() {

  const inputRef = useRef(null);

  const [state, dispatch] = useReducer(reducer, {
    currentUser: null,
    loading: true,
    rooms: {},
    currentRoomId: null
  })

  const onKeyDown = useCallback((event: KeyboardEvent) => {
    inputRef.current.focus()
    if (event.key === 'Enter') {
      const messageContent = inputRef.current?.value
      if (messageContent) {
        const evt = {
          type: 'messages',
          payload: {
            roomId: state.currentRoomId,
            messages: [{
              user: {
                id: state.currentUser.id,
                username: state.currentUser.username
              },
              content: messageContent
            }]
          }
        }
        websocketRef.current.send(JSON.stringify(evt))
        inputRef.current.value = ''
      }
    }
  }, [state.currentUser?.id, state.currentUser?.username, state.currentRoomId])

  useEffect(() => {
    window.addEventListener('keydown', onKeyDown);
    return () => {
      window.removeEventListener('keydown', onKeyDown);
    };
  }, [onKeyDown])

  const websocketRef = useRef<WebSocket>(null)

  useEffect(() => {
    websocketRef.current = new WebSocket('ws://localhost:8080')
    websocketRef.current.addEventListener('message', (event) => {
      const { type, payload } = JSON.parse(event.data)
      console.log(type, payload)
      switch (type) {
        case 'hello': dispatch({
          type: 'set_initial_state',
          payload: {
            currentUser: getStoredUser(),
            rooms: payload.rooms,
          }
        })
          break
        case 'messages': dispatch({
          type: 'add_messages',
          payload: {
            roomId: payload.roomId,
            messages: payload.messages
          }
        })
          break
      }
    })

    // Close the socket on page close. TODO: we should also deregister event handlers
    return () => {
      websocketRef.current.close()
    }
  }, [])

  const onClickRoom = useCallback((roomId: string) => {
    dispatch({ type: 'set_current_room', payload: { roomId } })
  }, [dispatch])

  if (state.loading) return <p>Loading...</p>

  const currentRoomMessages = state.rooms[state.currentRoomId]?.messages || []

  return (
    <>
      <Head>
        <title>BoostServerTech chat</title>
        <link rel="icon" href="/favicon.ico" />
      </Head>
      <div className="flex flex-col h-full">
        <Header />
        <div className="flex-1 flex min-h-0" style={{ borderTop: '1px solid var(--boost-light-grey)' }}>
          <div className='flex-1 flex flex-col overflow-y-scroll'>
            {Object.values(state.rooms).map(({ id, name, messages }) =>
              <RoomEntry
                key={id}
                id={id}
                name={name}
                selected={id === state.currentRoomId}
                lastMessage={messages[0]?.content || 'No messages yet'}
                onClick={onClickRoom}
              />
            )}
          </div>
          <div className='flex-[3] flex flex-col'>
            <div className='flex-1 flex flex-col-reverse p-5 overflow-y-scroll' style={{ backgroundColor: 'var(--boost-light-grey)' }}>
              {
                currentRoomMessages.length === 0 ?
                  <p>No messages</p> :
                  currentRoomMessages.map(msg => <Message
                    key={msg.id}
                    userId={msg.user.id}
                    username={msg.user.username}
                    content={msg.content}
                    timestamp={msg.timestamp}
                    currentUserId={state.currentUser.id}
                  />)
              }
            </div>
            <div className='flex'>
              <div className='flex-1 flex p-2' style={{ backgroundColor: 'var(--boost-light-grey)' }}>
                <input
                  type='text'
                  className='flex-1 text-xl pl-4 pr-4 pt-2 pb-2 border-0 rounded-xl'
                  placeholder='Type a message...'
                  ref={inputRef}
                />
              </div>
            </div>
          </div>
        </div>
      </div>
    </>
  )
}
